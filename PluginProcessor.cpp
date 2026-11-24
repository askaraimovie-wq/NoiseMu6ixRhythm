#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    inline float fastTanh (float x)
    {
        if (x < -3.0f) return -1.0f;
        if (x > 3.0f)  return  1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float hermiteSample (const float* src, int totalSamps, double pos)
    {
        int i1 = static_cast<int> (pos);
        float frac = static_cast<float> (pos - static_cast<double> (i1));
        int i0 = (i1 > 0) ? (i1 - 1) : 0;
        int i2 = (i1 + 1 < totalSamps) ? (i1 + 1) : (totalSamps - 1);
        int i3 = (i1 + 2 < totalSamps) ? (i1 + 2) : (totalSamps - 1);

        float y0 = src[i0];
        float y1 = src[i1];
        float y2 = src[i2];
        float y3 = src[i3];

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    inline float fastExpNeg (float x)
    {
        float t = 1.0f + x * 0.25f;
        float t2 = t * t;
        return 1.0f / (t2 * t2);
    }
}

SampleGroup::SampleGroup()
{
    static constexpr SimpleFxType fixedOrder[numFxSlots] = {
        SimpleFxType::EQ,
        SimpleFxType::Compressor,
        SimpleFxType::Saturation,
        SimpleFxType::Transient,
        SimpleFxType::Width,
        SimpleFxType::Delay,
        SimpleFxType::Reverb
    };
    for (int i = 0; i < numFxSlots; ++i)
        fxSlots[static_cast<size_t> (i)].setDefaultsForType (fixedOrder[i]);
}

void SimpleFxSlot::setDefaultsForType (SimpleFxType t)
{
    type = t;
    enabled = false;
    switch (t)
    {
        case SimpleFxType::EQ:         p1 = 0.0f;  p2 = 0.45f; p3 = 0.50f; p4 = 1.00f; p5 = 0.50f; break;
        case SimpleFxType::Compressor: p1 = 0.35f; p2 = 0.35f; p3 = 0.25f; p4 = 0.45f; p5 = 0.50f; break;
        case SimpleFxType::Saturation: p1 = 0.35f; p2 = 0.80f; p3 = 0.35f; p4 = 0.50f; p5 = 0.50f; break;
        case SimpleFxType::Transient:  p1 = 0.50f; p2 = 0.50f; p3 = 0.45f; p4 = 0.20f; p5 = 1.00f; break;
        case SimpleFxType::Width:      p1 = 0.65f; p2 = 0.22f; p3 = 0.28f; p4 = 0.25f; p5 = 0.60f; break;
        case SimpleFxType::Delay:      p1 = 0.35f; p2 = 0.35f; p3 = 0.65f; p4 = 0.50f; p5 = 0.25f; break;
        case SimpleFxType::Reverb:     p1 = 0.45f; p2 = 0.45f; p3 = 0.40f; p4 = 0.15f; p5 = 0.28f; break;
        default:                       p1 = p2 = p3 = p4 = p5 = 0.5f; break;
    }
}

void SimpleFxSlot::prepare (double sr)
{
    hpL.reset(); hpR.reset();
    lpL.reset(); lpR.reset();
    midL.reset(); midR.reset();
    env = 0.0f;
    envSlow = 0.0f;
    lastGrDb = 0.0f;
    lastInputLevel = 0.0f;
    chorusPhase = 0.0f;
    grHistory.fill (0.0f);
    grWriteIdx = 0;

    if (type == SimpleFxType::Delay || type == SimpleFxType::Width)
    {
        int len = (type == SimpleFxType::Delay)
            ? (static_cast<int> (sr * 1.5) + 64)
            : (static_cast<int> (sr * 0.06) + 64);
        bufL.assign (static_cast<size_t> (len), 0.0f);
        bufR.assign (static_cast<size_t> (len), 0.0f);
        writePos = 0;
    }
    else if (type == SimpleFxType::Reverb)
    {
        int preLen = static_cast<int> (sr * 0.16) + 64;
        bufL.assign (static_cast<size_t> (preLen), 0.0f);
        bufR.assign (static_cast<size_t> (preLen), 0.0f);
        writePos = 0;

        double scale = sr / 44100.0;
        static constexpr int baseCombL[4] = { 1117, 1277, 1423, 1559 };
        static constexpr int baseCombR[4] = { 1151, 1307, 1453, 1597 };
        static constexpr int baseApL[2]   = { 225, 556 };
        static constexpr int baseApR[2]   = { 241, 577 };

        for (int c = 0; c < 4; ++c)
        {
            int lLen = std::max (64, static_cast<int> (std::round (baseCombL[c] * scale * 1.65)));
            int rLen = std::max (64, static_cast<int> (std::round (baseCombR[c] * scale * 1.65)));
            combL[static_cast<size_t> (c)].assign (static_cast<size_t> (lLen), 0.0f);
            combR[static_cast<size_t> (c)].assign (static_cast<size_t> (rLen), 0.0f);
            combPosL[static_cast<size_t> (c)] = 0;
            combPosR[static_cast<size_t> (c)] = 0;
            combDampL[static_cast<size_t> (c)] = 0.0f;
            combDampR[static_cast<size_t> (c)] = 0.0f;
        }
        for (int a = 0; a < 2; ++a)
        {
            int lLen = std::max (32, static_cast<int> (std::round (baseApL[a] * scale)));
            int rLen = std::max (32, static_cast<int> (std::round (baseApR[a] * scale)));
            apL[static_cast<size_t> (a)].assign (static_cast<size_t> (lLen), 0.0f);
            apR[static_cast<size_t> (a)].assign (static_cast<size_t> (rLen), 0.0f);
            apPosL[static_cast<size_t> (a)] = 0;
            apPosR[static_cast<size_t> (a)] = 0;
        }
    }
}

juce::String SimpleFxSlot::getTypeName (SimpleFxType t)
{
    switch (t)
    {
        case SimpleFxType::EQ:         return "Pro-EQ";
        case SimpleFxType::Compressor: return "Pro-Comp";
        case SimpleFxType::Saturation: return "Saturn-Sat";
        case SimpleFxType::Transient:  return "Punch-Trans";
        case SimpleFxType::Width:      return "Space-Width";
        case SimpleFxType::Delay:      return "Timeless-Dly";
        case SimpleFxType::Reverb:     return "Pro-Rev";
        default:                       return "FX Off";
    }
}

juce::String SimpleFxSlot::getShortCardName (SimpleFxType t)
{
    switch (t)
    {
        case SimpleFxType::EQ:         return "EQ";
        case SimpleFxType::Compressor: return "COMP";
        case SimpleFxType::Saturation: return "SAT";
        case SimpleFxType::Transient:  return "TRANS";
        case SimpleFxType::Width:      return "WIDTH";
        case SimpleFxType::Delay:      return "DLY";
        case SimpleFxType::Reverb:     return "REV";
        default:                       return "OFF";
    }
}

juce::String SimpleFxSlot::getParamLabel (SimpleFxType t, int idx)
{
    switch (t)
    {
        case SimpleFxType::EQ:
            return idx == 0 ? "Low Cut" : (idx == 1 ? "Mid Freq" : (idx == 2 ? "Mid Gain" : (idx == 3 ? "High Cut" : "Out Gain")));
        case SimpleFxType::Compressor:
            return idx == 0 ? "Thresh" : (idx == 1 ? "Ratio" : (idx == 2 ? "Attack" : (idx == 3 ? "Release" : "Makeup")));
        case SimpleFxType::Saturation:
            return idx == 0 ? "Drive" : (idx == 1 ? "Tone" : (idx == 2 ? "Dynamics" : (idx == 3 ? "Level" : "Mix")));
        case SimpleFxType::Transient:
            return idx == 0 ? "Attack" : (idx == 1 ? "Sustain" : (idx == 2 ? "Speed" : (idx == 3 ? "Warmth" : "Mix")));
        case SimpleFxType::Width:
            return idx == 0 ? "Width" : (idx == 1 ? "Chorus" : (idx == 2 ? "Rate" : (idx == 3 ? "Spread" : "Mix")));
        case SimpleFxType::Delay:
            return idx == 0 ? "Time" : (idx == 1 ? "Feedback" : (idx == 2 ? "LP Cut" : (idx == 3 ? "Width" : "Mix")));
        case SimpleFxType::Reverb:
            return idx == 0 ? "Space" : (idx == 1 ? "Decay" : (idx == 2 ? "Damping" : (idx == 3 ? "PreDelay" : "Mix")));
        default:
            return "P" + juce::String (idx + 1);
    }
}

void SimpleFxSlot::processStereo (float* L, float* R, int numSamples, double sr)
{
    if (! enabled || type == SimpleFxType::Off || numSamples <= 0)
        return;

    if (type == SimpleFxType::EQ)
    {
        bool useHP = (p1 > 0.01f);
        bool useLP = (p4 < 0.99f);
        float lowCutHz  = 20.0f * std::pow (35.0f, p1);
        float midFreqHz = 180.0f * std::pow (40.0f, p2);
        float midGainDb = (p3 - 0.5f) * 24.0f;
        float midGainLin = std::pow (10.0f, midGainDb * 0.05f);
        bool useMid = (std::abs (midGainDb) > 0.15f);
        float highCutHz = 800.0f * std::pow (25.0f, p4);
        float outGain   = std::pow (10.0f, (p5 - 0.5f) * 12.0f * 0.05f);

        if (useHP) { hpL.setParams (lowCutHz, 0.707f, sr); hpR.setParams (lowCutHz, 0.707f, sr); }
        if (useMid) { midL.setParams (midFreqHz, 1.15f, sr); midR.setParams (midFreqHz, 1.15f, sr); }
        if (useLP) { lpL.setParams (highCutHz, 0.707f, sr); lpR.setParams (highCutHz, 0.707f, sr); }

        float midScale = (midGainLin - 1.0f) * midL.k;
        for (int i = 0; i < numSamples; ++i)
        {
            float xL = L[i];
            float xR = R[i];
            if (useHP) { xL = hpL.processHP (xL); xR = hpR.processHP (xR); }
            if (useMid)
            {
                float bpL = midL.processBP (xL);
                float bpR = midR.processBP (xR);
                xL += bpL * midScale;
                xR += bpR * midScale;
            }
            if (useLP) { xL = lpL.processLP (xL); xR = lpR.processLP (xR); }
            L[i] = xL * outGain;
            R[i] = xR * outGain;
        }
    }
    else if (type == SimpleFxType::Compressor)
    {
        float threshDb = -36.0f * p1;
        float thresh   = std::pow (10.0f, threshDb * 0.05f);
        float ratio    = 1.0f + p2 * 11.0f;
        float slope    = 1.0f - (1.0f / ratio);
        float atkCoef  = 0.004f + (1.0f - p3) * 0.14f;
        float relCoef  = 0.0003f + (1.0f - p4) * 0.014f;
        float makeup   = std::pow (10.0f, (p5 - 0.5f) * 18.0f * 0.05f);

        float minGr = 1.0f;
        float invThresh = 1.0f / std::max (1e-4f, thresh);
        for (int i = 0; i < numSamples; ++i)
        {
            float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            env += (pk > env ? atkCoef : relCoef) * (pk - env);
            float gr = 1.0f;
            if (env > thresh)
            {
                float overRatio = env * invThresh;
                float lnOver = (overRatio - 1.0f) / (1.0f + 0.42f * (overRatio - 1.0f));
                gr = fastExpNeg (slope * lnOver);
            }
            if (gr < minGr) minGr = gr;
            float gTotal = gr * makeup;
            L[i] *= gTotal;
            R[i] *= gTotal;
        }
        lastGrDb = juce::Decibels::gainToDecibels (minGr, -24.0f);
        grHistory[static_cast<size_t> (grWriteIdx)] = juce::jlimit (0.0f, 1.0f, -lastGrDb / 24.0f);
        grWriteIdx = (grWriteIdx + 1) % grHistSize;
    }
    else if (type == SimpleFxType::Saturation)
    {
        float baseDrive = 1.0f + p1 * 7.5f;
        float cutHz     = 1400.0f + p2 * 18600.0f;
        float dynAmt    = p3;
        float level     = 0.5f + p4 * 1.0f;
        float mix       = p5;
        bool useTone    = (p2 < 0.98f);
        if (useTone) { lpL.setParams (cutHz, 0.707f, sr); lpR.setParams (cutHz, 0.707f, sr); }

        float blkPk = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            if (pk > blkPk) blkPk = pk;
            env += (pk > env ? 0.04f : 0.002f) * (pk - env);

            float dynDrive = baseDrive * (1.0f + dynAmt * (env * 2.2f - 0.35f));
            dynDrive = juce::jlimit (0.8f, 12.0f, dynDrive);
            float invNorm = 1.0f / std::max (0.15f, fastTanh (dynDrive));

            float sL = fastTanh (L[i] * dynDrive) * invNorm * level;
            float sR = fastTanh (R[i] * dynDrive) * invNorm * level;
            if (useTone) { sL = lpL.processLP (sL); sR = lpR.processLP (sR); }
            L[i] = L[i] * (1.0f - mix) + sL * mix;
            R[i] = R[i] * (1.0f - mix) + sR * mix;
        }
        lastInputLevel = std::max (blkPk, lastInputLevel * 0.85f);
    }
    else if (type == SimpleFxType::Transient)
    {
        float atkAmt  = (p1 - 0.5f) * 2.4f;
        float susAmt  = (p2 - 0.5f) * 2.0f;
        float speed   = 0.35f + p3 * 1.65f;
        float warmth  = p4;
        float mix     = p5;

        float fastAtk = 0.045f * speed;
        float fastRel = 0.004f * speed;
        float slowAtk = 0.0025f * speed;
        float slowRel = 0.0005f * speed;

        for (int i = 0; i < numSamples; ++i)
        {
            float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            env     += (pk > env     ? fastAtk : fastRel) * (pk - env);
            envSlow += (pk > envSlow ? slowAtk : slowRel) * (pk - envSlow);

            float diff = env - envSlow;
            float shapeGain = 1.0f;
            if (diff > 0.0f)
                shapeGain += atkAmt * std::min (1.0f, diff * 4.5f);
            else
                shapeGain += susAmt * std::min (1.0f, (-diff) * 4.5f);
            shapeGain = juce::jlimit (0.20f, 2.80f, shapeGain);

            float sL = L[i] * shapeGain;
            float sR = R[i] * shapeGain;
            if (warmth > 0.02f)
            {
                float w = 1.0f + warmth * 2.0f;
                sL = fastTanh (sL * w) / fastTanh (w);
                sR = fastTanh (sR * w) / fastTanh (w);
            }
            L[i] = L[i] * (1.0f - mix) + sL * mix;
            R[i] = R[i] * (1.0f - mix) + sR * mix;
        }
    }
    else if (type == SimpleFxType::Width)
    {
        if (bufL.empty()) prepare (sr);
        int bufLen = static_cast<int> (bufL.size());
        if (bufLen < 64) return;

        float widthGain = p1 * 2.0f;
        float chDepthMs = p2 * 8.0f;
        float rateHz    = 0.15f + p3 * 3.5f;
        float haasMs    = p4 * 12.0f;
        float mix       = p5;

        float phaseInc  = static_cast<float> (juce::MathConstants<double>::twoPi * rateHz / sr);
        float baseDel   = static_cast<float> ((6.0f + haasMs) * 0.001 * sr);
        float depthSamp = static_cast<float> (chDepthMs * 0.001 * sr);

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = L[i];
            float inR = R[i];
            bufL[static_cast<size_t> (writePos)] = inL;
            bufR[static_cast<size_t> (writePos)] = inR;

            float lfo = std::sin (chorusPhase);
            chorusPhase += phaseInc;
            if (chorusPhase >= juce::MathConstants<float>::twoPi)
                chorusPhase -= juce::MathConstants<float>::twoPi;

            float dL = juce::jlimit (2.0f, static_cast<float> (bufLen - 3), baseDel + lfo * depthSamp);
            float dR = juce::jlimit (2.0f, static_cast<float> (bufLen - 3), baseDel - lfo * depthSamp + static_cast<float> (haasMs * 0.0005 * sr));

            auto readFrac = [bufLen] (const std::vector<float>& b, int wp, float d) -> float
            {
                float rp = static_cast<float> (wp) - d;
                while (rp < 0.0f) rp += static_cast<float> (bufLen);
                int i0 = static_cast<int> (rp) % bufLen;
                int i1 = (i0 + 1) % bufLen;
                float fr = rp - std::floor (rp);
                return b[static_cast<size_t> (i0)] + fr * (b[static_cast<size_t> (i1)] - b[static_cast<size_t> (i0)]);
            };

            float cL = readFrac (bufL, writePos, dL);
            float cR = readFrac (bufR, writePos, dR);
            writePos = (writePos + 1) % bufLen;

            float wL = inL * (1.0f - p2 * 0.45f) + cL * (p2 * 0.55f);
            float wR = inR * (1.0f - p2 * 0.45f) + cR * (p2 * 0.55f);

            float mid  = 0.5f * (wL + wR);
            float side = 0.5f * (wL - wR) * widthGain;
            float outL = mid + side;
            float outR = mid - side;

            L[i] = inL * (1.0f - mix) + outL * mix;
            R[i] = inR * (1.0f - mix) + outR * mix;
        }
    }
    else if (type == SimpleFxType::Delay)
    {
        if (bufL.empty()) prepare (sr);
        int bufLen = static_cast<int> (bufL.size());
        if (bufLen < 64) return;

        int dL = juce::jlimit (64, bufLen - 2, static_cast<int> ((0.04f + p1 * 0.78f) * sr));
        int dR = juce::jlimit (64, bufLen - 2, static_cast<int> (dL * (1.0f + p4 * 0.25f)));
        float fb = p2 * 0.78f;
        float dampHz = 1000.0f + p3 * 16500.0f;
        float mix = p5;

        lpL.setParams (dampHz, 0.707f, sr);
        lpR.setParams (dampHz, 0.707f, sr);

        for (int i = 0; i < numSamples; ++i)
        {
            int rL = (writePos - dL + bufLen) % bufLen;
            int rR = (writePos - dR + bufLen) % bufLen;
            float tL = lpL.processLP (bufL[static_cast<size_t> (rL)]);
            float tR = lpR.processLP (bufR[static_cast<size_t> (rR)]);
            bufL[static_cast<size_t> (writePos)] = L[i] + tR * fb;
            bufR[static_cast<size_t> (writePos)] = R[i] + tL * fb;
            writePos = (writePos + 1) % bufLen;
            L[i] = L[i] * (1.0f - mix * 0.40f) + tL * mix;
            R[i] = R[i] * (1.0f - mix * 0.40f) + tR * mix;
        }
    }
    else if (type == SimpleFxType::Reverb)
    {
        if (bufL.empty() || combL[0].empty()) prepare (sr);
        int preBufLen = static_cast<int> (bufL.size());
        if (preBufLen < 64) return;

        int preDelSamps = juce::jlimit (1, preBufLen - 2, static_cast<int> (p4 * 0.12f * sr));
        float spaceRatio = 0.55f + p1 * 0.44f;
        float fb = 0.62f + p2 * 0.31f;
        float damp = 0.12f + p3 * 0.68f;
        float dampInv = 1.0f - damp;
        float mix = p5;
        float dryGain = 1.0f - mix * 0.45f;
        float wetGain = mix * 0.55f;

        int cTapL[4], cTapR[4];
        for (int c = 0; c < 4; ++c)
        {
            int maxL = static_cast<int> (combL[static_cast<size_t> (c)].size());
            int maxR = static_cast<int> (combR[static_cast<size_t> (c)].size());
            cTapL[c] = juce::jlimit (32, maxL - 2, static_cast<int> (maxL * spaceRatio));
            cTapR[c] = juce::jlimit (32, maxR - 2, static_cast<int> (maxR * spaceRatio));
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = L[i];
            float inR = R[i];
            float monoIn = (inL + inR) * 0.5f;

            bufL[static_cast<size_t> (writePos)] = monoIn;
            int preIdx = (writePos - preDelSamps + preBufLen) % preBufLen;
            float preSig = bufL[static_cast<size_t> (preIdx)];
            writePos = (writePos + 1) % preBufLen;

            float sumL = 0.0f, sumR = 0.0f;
            for (int c = 0; c < 4; ++c)
            {
                auto cu = static_cast<size_t> (c);
                int lenL = static_cast<int> (combL[cu].size());
                int wpL  = combPosL[cu];
                int rpL  = (wpL - cTapL[c] + lenL) % lenL;
                float outCL = combL[cu][static_cast<size_t> (rpL)];
                combDampL[cu] = outCL * dampInv + combDampL[cu] * damp;
                combL[cu][static_cast<size_t> (wpL)] = preSig + combDampL[cu] * fb;
                combPosL[cu] = (wpL + 1) % lenL;
                sumL += outCL;

                int lenR = static_cast<int> (combR[cu].size());
                int wpR  = combPosR[cu];
                int rpR  = (wpR - cTapR[c] + lenR) % lenR;
                float outCR = combR[cu][static_cast<size_t> (rpR)];
                combDampR[cu] = outCR * dampInv + combDampR[cu] * damp;
                combR[cu][static_cast<size_t> (wpR)] = preSig + combDampR[cu] * fb;
                combPosR[cu] = (wpR + 1) % lenR;
                sumR += outCR;
            }

            float wL = sumL * 0.25f;
            float wR = sumR * 0.25f;
            for (int a = 0; a < 2; ++a)
            {
                auto au = static_cast<size_t> (a);
                int lenL = static_cast<int> (apL[au].size());
                int pL = apPosL[au];
                float bufOutL = apL[au][static_cast<size_t> (pL)];
                apL[au][static_cast<size_t> (pL)] = wL + bufOutL * 0.5f;
                wL = bufOutL - wL;
                apPosL[au] = (pL + 1) % lenL;

                int lenR = static_cast<int> (apR[au].size());
                int pR = apPosR[au];
                float bufOutR = apR[au][static_cast<size_t> (pR)];
                apR[au][static_cast<size_t> (pR)] = wR + bufOutR * 0.5f;
                wR = bufOutR - wR;
                apPosR[au] = (pR + 1) % lenR;
            }

            L[i] = inL * dryGain + wL * wetGain;
            R[i] = inR * dryGain + wR * wetGain;
        }
    }
}

juce::Colour RythmeAudioProcessor::getPaletteColour (int idx)
{
    static const juce::Colour palette[numPaletteColours] = {
        juce::Colour (0xff1b4fd8),
        juce::Colour (0xffc62828),
        juce::Colour (0xff6a1b9a),
        juce::Colour (0xff1b7a43),
        juce::Colour (0xff006978),
        juce::Colour (0xffc85000),
        juce::Colour (0xff7b1535),
        juce::Colour (0xff142864),
        juce::Colour (0xffad1457),
        juce::Colour (0xff9e6b00),
        juce::Colour (0xff303f9f),
        juce::Colour (0xff374151)
    };
    return palette[juce::jlimit (0, numPaletteColours - 1, idx)];
}

juce::String RythmeAudioProcessor::getPaletteColourName (int idx)
{
    static const char* names[numPaletteColours] = {
        "Deep Blue", "Crimson Red", "Royal Purple", "Forest Green",
        "Dark Teal", "Burnt Orange", "Maroon", "Navy",
        "Deep Magenta", "Dark Gold", "Indigo", "Charcoal"
    };
    return names[juce::jlimit (0, numPaletteColours - 1, idx)];
}

KinetikThemePalette RythmeAudioProcessor::getThemePalette (int themeIdx)
{
    static const KinetikThemePalette themes[numUiThemes] = {
        { "Classic B&W",     juce::Colour (0xff07080a), juce::Colour (0xff0d1016), juce::Colour (0xff141820),
                             juce::Colour (0xff222630), juce::Colour (0xfff2f4f8), juce::Colour (0xfff2f4f8),
                             juce::Colour (0xff9aa4b5), juce::Colour (0xfff2f4f8), false },
        { "Warm Paper",      juce::Colour (0xfff0e9dc), juce::Colour (0xffe5dccb), juce::Colour (0xffd8ceba),
                             juce::Colour (0xffc4b8a2), juce::Colour (0xff1d1a16), juce::Colour (0xff1d1a16),
                             juce::Colour (0xff5c5347), juce::Colour (0xff2b241d), true },
        { "Midnight Studio", juce::Colour (0xff080d18), juce::Colour (0xff0f182a), juce::Colour (0xff16223a),
                             juce::Colour (0xff243454), juce::Colour (0xffdbe9ff), juce::Colour (0xffeaf2ff),
                             juce::Colour (0xff88a2cc), juce::Colour (0xff4ca3ff), false },
        { "Analog Amber",    juce::Colour (0xff0d0a07), juce::Colour (0xff17120c), juce::Colour (0xff221a11),
                             juce::Colour (0xff36291a), juce::Colour (0xffffcb74), juce::Colour (0xffffdfb0),
                             juce::Colour (0xffb8915a), juce::Colour (0xfff29f24), false },
        { "Emerald Console", juce::Colour (0xff07100c), juce::Colour (0xff0d1c15), juce::Colour (0xff142920),
                             juce::Colour (0xff224032), juce::Colour (0xffb8f5d6), juce::Colour (0xffd8ffeb),
                             juce::Colour (0xff6ca88a), juce::Colour (0xff2fd187), false },
        { "Crimson Dark",    juce::Colour (0xff10080a), juce::Colour (0xff1b0e12), juce::Colour (0xff26151b),
                             juce::Colour (0xff3d222b), juce::Colour (0xffffcfd6), juce::Colour (0xffffe5e9),
                             juce::Colour (0xffb87c86), juce::Colour (0xffff4764), false },
        { "Graphite Gold",   juce::Colour (0xff141518), juce::Colour (0xff1d1f24), juce::Colour (0xff272a31),
                             juce::Colour (0xff383c45), juce::Colour (0xffeed79f), juce::Colour (0xfff6e7c1),
                             juce::Colour (0xffa69b80), juce::Colour (0xffe0b85c), false },
        { "Cyber Slate",     juce::Colour (0xff0b1116), juce::Colour (0xff121d26), juce::Colour (0xff192936),
                             juce::Colour (0xff263d4f), juce::Colour (0xffc2f3ff), juce::Colour (0xffe0f9ff),
                             juce::Colour (0xff72a2b5), juce::Colour (0xff2cd8f5), false }
    };
    return themes[juce::jlimit (0, numUiThemes - 1, themeIdx)];
}

float RythmeAudioProcessor::vol100ToLinearGain (float vol100)
{
    float v = juce::jlimit (0.0f, 100.0f, vol100);
    if (v <= 0.01f) return 0.0f;
    return std::pow (v / 80.0f, 1.35f);
}

RythmeAudioProcessor::RythmeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    heldNotesVel.fill (0.0f);
    sustainedNotes.fill (false);
    for (auto& a : activeNotesUi) a.store (0);
    scopeRing.fill (0.0f);
    fftRing.fill (0.0f);
    groups.emplace_back();
    selectedGroupIndex = 0;
    loadLibraryRegistry();
    loadUiLayoutFromDisk();
    recomputeSampleRamMb();
}

RythmeAudioProcessor::~RythmeAudioProcessor() = default;

void RythmeAudioProcessor::copyScopeHistory (std::array<float, scopeHistorySize>& dest, bool& isSounding) const
{
    const juce::SpinLock::ScopedLockType sl (scopeLock);
    int rIdx = scopeWriteIdx;
    for (int i = 0; i < scopeHistorySize; ++i)
    {
        dest[static_cast<size_t> (i)] = scopeRing[static_cast<size_t> (rIdx)];
        rIdx = (rIdx + 1) % scopeHistorySize;
    }
    isSounding = (silentSamplesHold < static_cast<int> (currentSampleRate * 0.14));
}

void RythmeAudioProcessor::copyRecentAudioForFft (std::array<float, fftSize>& dest, bool& isSounding) const
{
    int w = fftWriteIdx.load (std::memory_order_acquire);
    int start = (w - fftSize + fftRingSize) % fftRingSize;
    for (int i = 0; i < fftSize; ++i)
        dest[static_cast<size_t> (i)] = fftRing[static_cast<size_t> ((start + i) % fftRingSize)];
    isSounding = (silentSamplesHold < static_cast<int> (currentSampleRate * 0.18));
}

void RythmeAudioProcessor::recomputeSampleRamMb()
{
    const juce::ScopedLock sl (zoneLock);
    uint64_t totalBytes = 0;
    for (const auto& z : zones)
        for (const auto& l : z.layers)
            totalBytes += static_cast<uint64_t> (l.buffer.getNumChannels())
                        * static_cast<uint64_t> (l.buffer.getNumSamples())
                        * sizeof (float);
    sampleRamMb.store (static_cast<float> (totalBytes) / (1024.0f * 1024.0f));
}

juce::File RythmeAudioProcessor::getRegistryFile() const
{
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("NoiseMu6ixStudio")
                       .getChildFile ("KINETIK");
    appData.createDirectory();
    return appData.getChildFile ("library_registry.xml");
}

juce::File RythmeAudioProcessor::getUiLayoutFile() const
{
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("NoiseMu6ixStudio")
                       .getChildFile ("KINETIK");
    appData.createDirectory();
    return appData.getChildFile ("ui_layout.xml");
}

void RythmeAudioProcessor::saveUiLayoutToDisk()
{
    juce::XmlElement root ("KINETIK_UI_LAYOUT");
    root.setAttribute ("schemaVersion", 3);
    root.setAttribute ("uiScale", static_cast<double> (uiScale));
    root.setAttribute ("uiThemeId", uiThemeId);
    root.setAttribute ("customizeGridStep", customizeGridStep);
    root.setAttribute ("logoStyleId", logoStyleId);
    root.setAttribute ("logoColourIdx", logoColourIdx);
    root.setAttribute ("knobStyleId", knobStyleId);
    root.setAttribute ("knobColourIdx", knobColourIdx);
    root.setAttribute ("faderStyleId", faderStyleId);
    root.setAttribute ("faderColourIdx", faderColourIdx);
    root.setAttribute ("switchStyleId", switchStyleId);
    root.setAttribute ("switchColourIdx", switchColourIdx);

    auto* mb = root.createNewChildElement ("MAIN_BOUNDS");
    for (const auto& kv : mainCustomBounds)
    {
        auto* e = mb->createNewChildElement ("B");
        e->setAttribute ("k", kv.first);
        e->setAttribute ("x", kv.second.getX());
        e->setAttribute ("y", kv.second.getY());
        e->setAttribute ("w", kv.second.getWidth());
        e->setAttribute ("h", kv.second.getHeight());
    }

    auto* eb = root.createNewChildElement ("EDIT_BOUNDS");
    for (const auto& kv : editCustomBounds)
    {
        auto* e = eb->createNewChildElement ("B");
        e->setAttribute ("k", kv.first);
        e->setAttribute ("x", kv.second.getX());
        e->setAttribute ("y", kv.second.getY());
        e->setAttribute ("w", kv.second.getWidth());
        e->setAttribute ("h", kv.second.getHeight());
    }

    auto* mh = root.createNewChildElement ("MAIN_HIDDEN");
    for (const auto& k : mainHiddenKeys)
        mh->createNewChildElement ("H")->setAttribute ("k", k);

    auto* eh = root.createNewChildElement ("EDIT_HIDDEN");
    for (const auto& k : editHiddenKeys)
        eh->createNewChildElement ("H")->setAttribute ("k", k);

    root.writeTo (getUiLayoutFile());
}

void RythmeAudioProcessor::loadUiLayoutFromDisk()
{
    auto f = getUiLayoutFile();
    if (! f.existsAsFile()) return;
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("KINETIK_UI_LAYOUT")) return;

    int schemaVer     = xml->getIntAttribute ("schemaVersion", 1);
    float loadedScale = static_cast<float> (xml->getDoubleAttribute ("uiScale", 1.0));
    if (schemaVer < 2 && loadedScale < 0.70f)
        loadedScale = 1.0f;
    uiScale           = juce::jlimit (0.5f, 1.5f, loadedScale);
    uiThemeId         = juce::jlimit (0, numUiThemes - 1, xml->getIntAttribute ("uiThemeId", 0));
    customizeGridStep = juce::jlimit (0, 50, xml->getIntAttribute ("customizeGridStep", 20));
    logoStyleId       = juce::jlimit (1, 4, xml->getIntAttribute ("logoStyleId", 1));
    logoColourIdx     = juce::jlimit (0, numPaletteColours - 1, xml->getIntAttribute ("logoColourIdx", 0));
    knobStyleId       = juce::jlimit (1, 10, xml->getIntAttribute ("knobStyleId", 1));
    knobColourIdx     = juce::jlimit (0, numPaletteColours - 1, xml->getIntAttribute ("knobColourIdx", 0));
    faderStyleId      = juce::jlimit (1, 10, xml->getIntAttribute ("faderStyleId", 1));
    faderColourIdx    = juce::jlimit (0, numPaletteColours - 1, xml->getIntAttribute ("faderColourIdx", 0));
    switchStyleId     = juce::jlimit (1, 10, xml->getIntAttribute ("switchStyleId", 1));
    switchColourIdx   = juce::jlimit (0, numPaletteColours - 1, xml->getIntAttribute ("switchColourIdx", 0));

    mainCustomBounds.clear();
    if (auto* mb = xml->getChildByName ("MAIN_BOUNDS"))
    {
        for (auto* e : mb->getChildIterator())
        {
            if (e->hasTagName ("B"))
            {
                auto key = e->getStringAttribute ("k");
                int x = e->getIntAttribute ("x", 0);
                int y = e->getIntAttribute ("y", 0);
                int w = e->getIntAttribute ("w", 40);
                int h = e->getIntAttribute ("h", 24);
                if (key.isNotEmpty() && w >= 16 && h >= 12)
                    mainCustomBounds[key] = juce::Rectangle<int> (x, y, w, h);
            }
        }
    }

    editCustomBounds.clear();
    if (schemaVer >= 3)
    {
        if (auto* eb = xml->getChildByName ("EDIT_BOUNDS"))
        {
            for (auto* e : eb->getChildIterator())
            {
                if (e->hasTagName ("B"))
                {
                    auto key = e->getStringAttribute ("k");
                    int x = e->getIntAttribute ("x", 0);
                    int y = e->getIntAttribute ("y", 0);
                    int w = e->getIntAttribute ("w", 40);
                    int h = e->getIntAttribute ("h", 24);
                    if (key.isNotEmpty() && w >= 16 && h >= 12)
                        editCustomBounds[key] = juce::Rectangle<int> (x, y, w, h);
                }
            }
        }
    }

    mainHiddenKeys.clear();
    if (auto* mh = xml->getChildByName ("MAIN_HIDDEN"))
        for (auto* e : mh->getChildIterator())
            if (e->hasTagName ("H") && e->getStringAttribute ("k").isNotEmpty())
                mainHiddenKeys.insert (e->getStringAttribute ("k"));

    editHiddenKeys.clear();
    if (schemaVer >= 3)
    {
        if (auto* eh = xml->getChildByName ("EDIT_HIDDEN"))
            for (auto* e : eh->getChildIterator())
                if (e->hasTagName ("H") && e->getStringAttribute ("k").isNotEmpty())
                    editHiddenKeys.insert (e->getStringAttribute ("k"));
    }
}

void RythmeAudioProcessor::refreshLibraryFolderIfMatches (const juce::File& file)
{
    const juce::ScopedLock sl (zoneLock);
    if (rootLibraryFolder.isDirectory())
    {
        scanFolderIntoCategories (rootLibraryFolder);
        saveLibraryRegistry();
    }
    else if (file.getParentDirectory().isDirectory())
    {
        addLibraryFolderPersistent (file.getParentDirectory());
    }
}

void RythmeAudioProcessor::scanFolderIntoCategories (const juce::File& folder)
{
    libraryCategories.clear();
    if (! folder.isDirectory()) return;

    juce::Array<juce::File> subDirs;
    folder.findChildFiles (subDirs, juce::File::findDirectories, false);
    subDirs.sort();

    for (const auto& sd : subDirs)
    {
        LibraryCategoryEntry cat;
        cat.name = sd.getFileName();
        cat.folder = sd;
        cat.expanded = false;
        for (const auto& f : sd.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
            cat.items.push_back (f);
        std::sort (cat.items.begin(), cat.items.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
        });
        libraryCategories.push_back (std::move (cat));
    }

    std::vector<juce::File> rootItems;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aiff;*.flac;*.mn6x"))
        rootItems.push_back (f);
    if (! rootItems.empty())
    {
        std::sort (rootItems.begin(), rootItems.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
        });
        LibraryCategoryEntry rootCat;
        rootCat.name = folder.getFileName();
        rootCat.folder = folder;
        rootCat.expanded = true;
        rootCat.items = std::move (rootItems);
        libraryCategories.insert (libraryCategories.begin(), std::move (rootCat));
    }
}

void RythmeAudioProcessor::setRootLibraryFolder (const juce::File& folder)
{
    if (! folder.isDirectory()) return;
    const juce::ScopedLock sl (zoneLock);
    rootLibraryFolder = folder;
    scanFolderIntoCategories (folder);
    saveLibraryRegistry();
}

void RythmeAudioProcessor::saveLibraryRegistry()
{
    juce::XmlElement root ("KINETIK_LIBRARY");
    root.setAttribute ("rootFolder", rootLibraryFolder.getFullPathName());
    for (const auto& cat : libraryCategories)
    {
        auto* cx = root.createNewChildElement ("CATEGORY");
        cx->setAttribute ("name", cat.name);
        cx->setAttribute ("folder", cat.folder.getFullPathName());
        cx->setAttribute ("expanded", cat.expanded ? 1 : 0);
        for (const auto& item : cat.items)
            cx->createNewChildElement ("ITEM")->setAttribute ("file", item.getFullPathName());
    }
    root.writeTo (getRegistryFile());
}

void RythmeAudioProcessor::loadLibraryRegistry()
{
    libraryCategories.clear();
    auto regFile = getRegistryFile();
    if (! regFile.existsAsFile()) return;
    auto xml = juce::XmlDocument::parse (regFile);
    if (xml == nullptr) return;

    rootLibraryFolder = juce::File (xml->getStringAttribute ("rootFolder"));
    if (rootLibraryFolder.isDirectory())
    {
        scanFolderIntoCategories (rootLibraryFolder);
        return;
    }

    for (auto* cx : xml->getChildIterator())
    {
        if (! cx->hasTagName ("CATEGORY")) continue;
        LibraryCategoryEntry cat;
        cat.name = cx->getStringAttribute ("name", "LIBRARY");
        cat.folder = juce::File (cx->getStringAttribute ("folder"));
        cat.expanded = cx->getIntAttribute ("expanded", 0) != 0;

        for (auto* ix : cx->getChildIterator())
        {
            if (ix->hasTagName ("ITEM"))
            {
                juce::File f (ix->getStringAttribute ("file"));
                if (f.existsAsFile()) cat.items.push_back (f);
            }
        }
        if (cat.items.empty() && cat.folder.isDirectory())
            for (const auto& f : cat.folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
                cat.items.push_back (f);
        if (! cat.items.empty() || cat.folder.isDirectory())
            libraryCategories.push_back (std::move (cat));
    }
}

void RythmeAudioProcessor::addLibraryFolderPersistent (const juce::File& folder)
{
    if (! folder.isDirectory()) return;
    const juce::ScopedLock sl (zoneLock);
    for (auto& existing : libraryCategories)
    {
        if (existing.folder == folder)
        {
            existing.items.clear();
            for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
                existing.items.push_back (f);
            existing.expanded = true;
            saveLibraryRegistry();
            return;
        }
    }
    LibraryCategoryEntry cat;
    cat.name = folder.getFileName();
    cat.folder = folder;
    cat.expanded = true;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
        cat.items.push_back (f);
    libraryCategories.push_back (std::move (cat));
    saveLibraryRegistry();
}

void RythmeAudioProcessor::sortLibraryPersistent()
{
    const juce::ScopedLock sl (zoneLock);
    std::sort (libraryCategories.begin(), libraryCategories.end(),
        [] (const LibraryCategoryEntry& a, const LibraryCategoryEntry& b)
        {
            return a.name.compareIgnoreCase (b.name) < 0;
        });
    for (auto& cat : libraryCategories)
    {
        std::sort (cat.items.begin(), cat.items.end(),
            [] (const juce::File& a, const juce::File& b)
            {
                return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
            });
    }
    saveLibraryRegistry();
}

void RythmeAudioProcessor::removeLibraryEntryPersistent (int catIdx)
{
    const juce::ScopedLock sl (zoneLock);
    if (catIdx >= 0 && catIdx < static_cast<int> (libraryCategories.size()))
    {
        libraryCategories.erase (libraryCategories.begin() + catIdx);
        saveLibraryRegistry();
    }
}

void RythmeAudioProcessor::toggleLibraryCategoryExpanded (int catIdx)
{
    const juce::ScopedLock sl (zoneLock);
    if (catIdx >= 0 && catIdx < static_cast<int> (libraryCategories.size()))
    {
        libraryCategories[static_cast<size_t> (catIdx)].expanded = ! libraryCategories[static_cast<size_t> (catIdx)].expanded;
        saveLibraryRegistry();
    }
}

void RythmeAudioProcessor::setLoadedItemInPanel (const juce::String& displayName, const juce::File& file, int keyHint)
{
    loadedItemActive = true;
    loadedItemName = displayName;
    loadedItemFile = file;
    loadedItemKey = keyHint;
}

void RythmeAudioProcessor::clearLoadedItemPanel()
{
    const juce::ScopedLock sl (zoneLock);
    if (loadedItemKey >= 0)
    {
        for (int i = static_cast<int> (zones.size()) - 1; i >= 0; --i)
        {
            if (zones[static_cast<size_t> (i)].rootKey == loadedItemKey)
            {
                deleteZone (i);
                break;
            }
        }
    }
    loadedItemActive = false;
    loadedItemName.clear();
    loadedItemFile = juce::File();
    loadedItemKey = -1;
}

std::vector<RhythmEvent> RythmeAudioProcessor::createDefaultEvents() const
{
    return { { 0.000f, 0.95f }, { 0.250f, 0.85f }, { 0.500f, 0.95f }, { 0.750f, 0.85f } };
}

juce::AudioProcessorValueTreeState::ParameterLayout RythmeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> ("frr", "FRR (Fake Round Robin)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("fvl", "FVL (Fake Velocity Layer)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("pitchTrack", "Pitch Track (Keys)", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "playMode", "Play Mode", juce::StringArray { "POLY", "MONO", "LEGATO" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("glideMs", "Glide (ms)", 0.0f, 400.0f, 45.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("seqLoop", "Rhythm Loop", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("hostSync", "DAW Sync", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("internalBpm", "Internal BPM", 40.0f, 240.0f, 120.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("shuffle", "Shuffle", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("timeRand", "Time Randomize", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("velRand", "Velocity Randomize", 0.0f, 1.0f, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "grid", "Grid",
        juce::StringArray { "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)", "1/32", "1/48 (T)", "1/64" }, 1));

    return { params.begin(), params.end() };
}

int RythmeAudioProcessor::getGridDivisions() const
{
    int idx = static_cast<int> (apvts.getRawParameterValue ("grid")->load());
    switch (idx)
    {
        case 0: return 4;
        case 1: return 8;
        case 2: return 12;
        case 3: return 16;
        case 4: return 24;
        case 5: return 32;
        case 6: return 48;
        case 7: return 64;
        default: return 8;
    }
}

void RythmeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = std::max (22050.0, sampleRate);
    groupScratchBuf.setSize (2, std::max (512, samplesPerBlock));

    for (auto& v : voices)
    {
        v.filterL.reset();
        v.filterR.reset();
        v.active = false;
        v.isPreview = false;
    }
    sustainPedalDown = false;
    sustainedNotes.fill (false);
    heldNotesVel.fill (0.0f);
    legatoNoteStack.clear();
    previewActive.store (false);
    previewNote = -1;
    selectedZonePlayheadNorm.store (-1.0f);

    const juce::ScopedLock sl (zoneLock);
    for (auto& g : groups)
    {
        g.silentSamples = 999999;
        for (auto& slot : g.fxSlots)
            slot.prepare (currentSampleRate);
    }
}

void RythmeAudioProcessor::releaseResources() {}

bool RythmeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

int RythmeAudioProcessor::createEmptyGroup()
{
    const juce::ScopedLock sl (zoneLock);
    int nextNum = static_cast<int> (groups.size()) + 1;
    SampleGroup g;
    g.name = "Group " + juce::String (nextNum);
    for (auto& slot : g.fxSlots)
        slot.prepare (currentSampleRate);
    groups.push_back (std::move (g));
    selectedGroupIndex = static_cast<int> (groups.size()) - 1;
    return selectedGroupIndex;
}

bool RythmeAudioProcessor::deleteSelectedGroup()
{
    const juce::ScopedLock sl (zoneLock);
    if (groups.size() <= 1 || selectedGroupIndex < 0 || selectedGroupIndex >= static_cast<int> (groups.size()))
        return false;

    int delIdx = selectedGroupIndex;
    for (int i = static_cast<int> (zones.size()) - 1; i >= 0; --i)
    {
        if (zones[static_cast<size_t> (i)].groupIndex == delIdx)
            deleteZone (i);
        else if (zones[static_cast<size_t> (i)].groupIndex > delIdx)
            zones[static_cast<size_t> (i)].groupIndex--;
    }

    groups.erase (groups.begin() + delIdx);
    selectedGroupIndex = juce::jlimit (0, static_cast<int> (groups.size()) - 1, delIdx - 1);
    recomputeSampleRamMb();
    return true;
}

bool RythmeAudioProcessor::isKeyswitchNote (int midiNote) const
{
    for (const auto& g : groups)
        if (g.keyswitchNote >= 0 && g.keyswitchNote == midiNote)
            return true;
    return false;
}

int RythmeAudioProcessor::createEmptyZone (int key)
{
    const juce::ScopedLock sl (zoneLock);
    if (groups.empty())
    {
        groups.emplace_back();
        selectedGroupIndex = 0;
    }
    int k = juce::jlimit (0, 120, key);
    SampleZone z;
    z.name = "Key " + juce::MidiMessage::getMidiNoteName (k, true, true, 5);
    z.groupIndex = juce::jlimit (0, static_cast<int> (groups.size()) - 1, selectedGroupIndex);
    z.lowKey = k;
    z.highKey = k;
    z.rootKey = k;
    z.lowVel = 0;
    z.highVel = 127;
    z.realRREnabled = true;
    z.autoLoopZeroCross = true;
    z.loopCrossfade = 0.08f;
    z.oneShot = false;
    z.chokeGroup = 0;
    z.seqEvents = createDefaultEvents();
    z.colourIndex = 0;
    z.colour = getPaletteColour (0);
    zones.push_back (std::move (z));
    selectedZoneIndex = static_cast<int> (zones.size()) - 1;
    return selectedZoneIndex;
}

bool RythmeAudioProcessor::addSampleToZone (int zoneIndex, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    SampleLayer layer;
    layer.name = file.getFileNameWithoutExtension();
    layer.file = file;
    layer.sampleRate = reader->sampleRate;
    layer.buffer.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    reader->read (&layer.buffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    {
        const juce::ScopedLock sl (zoneLock);
        if (zoneIndex < 0 || zoneIndex >= static_cast<int> (zones.size()))
            return false;

        auto& z = zones[static_cast<size_t> (zoneIndex)];
        if (z.layers.empty())
            z.name = layer.name;
        z.layers.push_back (std::move (layer));
        z.selectedLayerIndex = static_cast<int> (z.layers.size()) - 1;
        if (z.layers.size() > 1)
            z.realRREnabled = true;
    }
    recomputeSampleRamMb();
    return true;
}

bool RythmeAudioProcessor::loadSampleIntoNewZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel, int highVel)
{
    int idx = createEmptyZone (rootKey);
    {
        const juce::ScopedLock sl (zoneLock);
        auto& z = zones[static_cast<size_t> (idx)];
        z.lowKey = juce::jlimit (0, 120, lowKey);
        z.highKey = juce::jlimit (z.lowKey, 120, highKey);
        z.rootKey = juce::jlimit (0, 120, rootKey);
        z.lowVel = juce::jlimit (0, 127, lowVel);
        z.highVel = juce::jlimit (z.lowVel, 127, highVel);
    }
    return addSampleToZone (idx, file);
}

bool RythmeAudioProcessor::deleteLayerFromZone (int zoneIndex, int layerIndex)
{
    {
        const juce::ScopedLock sl (zoneLock);
        if (zoneIndex < 0 || zoneIndex >= static_cast<int> (zones.size()))
            return false;
        auto& z = zones[static_cast<size_t> (zoneIndex)];
        if (layerIndex < 0 || layerIndex >= static_cast<int> (z.layers.size()))
            return false;

        for (auto& v : voices)
        {
            if (v.active && v.zoneIndex == zoneIndex)
            {
                if (v.layerIndex == layerIndex) v.active = false;
                else if (v.layerIndex > layerIndex) v.layerIndex--;
            }
        }
        z.layers.erase (z.layers.begin() + layerIndex);
        if (z.layers.empty())
        {
            z.selectedLayerIndex = 0;
            z.rrPlayIndex = 0;
            z.lastPlayedLayerIdx = -1;
        }
        else
        {
            z.selectedLayerIndex = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, layerIndex - 1);
            z.rrPlayIndex = z.rrPlayIndex % static_cast<int> (z.layers.size());
            z.name = z.layers[static_cast<size_t> (z.selectedLayerIndex)].name;
        }
    }
    recomputeSampleRamMb();
    return true;
}

void RythmeAudioProcessor::deleteZone (int index)
{
    {
        const juce::ScopedLock sl (zoneLock);
        if (index >= 0 && index < static_cast<int> (zones.size()))
        {
            for (auto& v : voices)
                if (v.zoneIndex == index) v.active = false;
                else if (v.zoneIndex > index) v.zoneIndex--;
            zones.erase (zones.begin() + index);
            if (zones.empty())
                selectedZoneIndex = -1;
            else
                selectedZoneIndex = juce::jlimit (0, static_cast<int> (zones.size()) - 1, selectedZoneIndex);
        }
    }
    recomputeSampleRamMb();
}

void RythmeAudioProcessor::deleteZones (std::vector<int> indices)
{
    std::sort (indices.begin(), indices.end(), std::greater<int>());
    indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
    for (int idx : indices)
        deleteZone (idx);
}

void RythmeAudioProcessor::normalizeSelectedSample()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex < 0 || selectedZoneIndex >= static_cast<int> (zones.size())) return;
    auto& z = zones[static_cast<size_t> (selectedZoneIndex)];
    if (z.layers.empty()) return;
    int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    auto& buf = z.layers[static_cast<size_t> (lIdx)].buffer;
    float peak = buf.getMagnitude (0, buf.getNumSamples());
    if (peak > 1e-5f)
        buf.applyGain (0.98f / peak);
}

void RythmeAudioProcessor::resetSelectedZoneDefaults()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex < 0 || selectedZoneIndex >= static_cast<int> (zones.size())) return;
    auto& z = zones[static_cast<size_t> (selectedZoneIndex)];
    z.startNorm = 0.0f;
    z.endNorm = 1.0f;
    z.loopStartNorm = 0.15f;
    z.loopEndNorm = 0.85f;
    z.loopCrossfade = 0.08f;
    z.fadeInNorm = 0.0f;
    z.fadeOutNorm = 0.0f;
    z.oneShot = false;
    z.chokeGroup = 0;
    z.pitchSemi = 0.0f;
    z.pan50 = 0.0f;
    z.volumeDb100 = 80.0f;
    z.attackMs = 0.10f;
    z.decayMs = 300.0f;
    z.sustainPct = 85.0f;
    z.releaseMs = 250.0f;
    z.cutoffHz = 20000.0f;
    z.resQ = 0.707f;
}

void RythmeAudioProcessor::clearSelectedZoneRhythm()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int> (zones.size()))
        zones[static_cast<size_t> (selectedZoneIndex)].seqEvents.clear();
}

float RythmeAudioProcessor::snapNormToZeroCrossing (int zoneIndex, float rawNorm) const
{
    rawNorm = juce::jlimit (0.0f, 1.0f, rawNorm);
    if (zoneIndex < 0 || zoneIndex >= static_cast<int> (zones.size()))
        return rawNorm;
    const auto& z = zones[static_cast<size_t> (zoneIndex)];
    if (! z.autoLoopZeroCross || z.layers.empty())
        return rawNorm;

    int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    const auto& buf = z.layers[static_cast<size_t> (lIdx)].buffer;
    int total = buf.getNumSamples();
    if (total < 16) return rawNorm;

    const float* d = buf.getReadPointer (0);
    int target = juce::jlimit (1, total - 2, static_cast<int> (rawNorm * total));
    int searchRadius = std::min (800, total / 6);

    for (int off = 0; off < searchRadius; ++off)
    {
        if (target + off < total - 1 && (d[target + off] <= 0.0f && d[target + off + 1] > 0.0f))
            return static_cast<float> (target + off) / static_cast<float> (total);
        if (target - off > 1 && (d[target - off - 1] <= 0.0f && d[target - off] > 0.0f))
            return static_cast<float> (target - off) / static_cast<float> (total);
    }
    for (int off = 0; off < searchRadius; ++off)
    {
        if (target + off < total - 1 && (d[target + off] * d[target + off + 1] <= 0.0f))
            return static_cast<float> (target + off) / static_cast<float> (total);
        if (target - off > 1 && (d[target - off] * d[target - off - 1] <= 0.0f))
            return static_cast<float> (target - off) / static_cast<float> (total);
    }
    return rawNorm;
}

bool RythmeAudioProcessor::hasAnySoloedGroup() const
{
    for (const auto& g : groups)
        if (g.soloed) return true;
    return false;
}

bool RythmeAudioProcessor::isGroupActiveForNote (int groupIdx) const
{
    if (groupIdx < 0 || groupIdx >= static_cast<int> (groups.size()))
        return false;
    const auto& g = groups[static_cast<size_t> (groupIdx)];
    if (g.muted)
        return false;
    if (hasAnySoloedGroup() && ! g.soloed)
        return false;

    if (g.keyswitchNote >= 0)
    {
        int activeKs = activeKeyswitchNote.load();
        if (activeKs >= 0 && g.keyswitchNote != activeKs)
            return false;
        if (activeKs < 0)
        {
            for (int i = 0; i < static_cast<int> (groups.size()); ++i)
                if (groups[static_cast<size_t> (i)].keyswitchNote >= 0)
                    return groups[static_cast<size_t> (i)].keyswitchNote == g.keyswitchNote;
        }
    }
    return true;
}

void RythmeAudioProcessor::startVoice (int zoneIdx, int midiNote, float velocity, bool isPreviewVoice)
{
    auto& z = zones[static_cast<size_t> (zoneIdx)];
    if (! isGroupActiveForNote (z.groupIndex))
        return;
    if (z.layers.empty())
        return;

    int playMode = static_cast<int> (apvts.getRawParameterValue ("playMode")->load());

    if (z.chokeGroup > 0)
    {
        for (auto& cv : voices)
            if (cv.active && cv.chokeGroup == z.chokeGroup && cv.envStage != 4)
                cv.envStage = 4;
    }

    if (! isPreviewVoice && playMode == 1)
    {
        for (auto& mv : voices)
            if (mv.active && ! mv.isPreview && mv.envStage != 4)
                mv.envStage = 4;
    }

    int numLayers = static_cast<int> (z.layers.size());
    int layerIdx = juce::jlimit (0, numLayers - 1, z.selectedLayerIndex);
    if (numLayers > 1 && z.realRREnabled)
    {
        layerIdx = z.rrPlayIndex % numLayers;
        if (layerIdx == z.lastPlayedLayerIdx)
            layerIdx = (layerIdx + 1) % numLayers;
        z.rrPlayIndex = (layerIdx + 1) % numLayers;
    }
    z.lastPlayedLayerIdx = layerIdx;

    const auto& layer = z.layers[static_cast<size_t> (layerIdx)];
    int totalSamps = layer.buffer.getNumSamples();
    if (totalSamps < 2)
        return;

    int vIdx = -1;
    for (int i = 0; i < maxVoices; ++i)
    {
        if (! voices[static_cast<size_t> (i)].active)
        {
            vIdx = i;
            break;
        }
    }
    if (vIdx < 0)
    {
        float minEnv = 99.0f;
        for (int i = 0; i < maxVoices; ++i)
        {
            const auto& cand = voices[static_cast<size_t> (i)];
            if (cand.envStage >= 3 && cand.envVal < minEnv)
            {
                minEnv = cand.envVal;
                vIdx = i;
            }
        }
    }
    if (vIdx < 0)
    {
        uint64_t oldest = UINT64_MAX;
        vIdx = 0;
        for (int i = 0; i < maxVoices; ++i)
        {
            if (voices[static_cast<size_t> (i)].ageOrder < oldest)
            {
                oldest = voices[static_cast<size_t> (i)].ageOrder;
                vIdx = i;
            }
        }
    }

    auto& v = voices[static_cast<size_t> (vIdx)];
    v.active = true;
    v.isPreview = isPreviewVoice;
    v.zoneIndex = zoneIdx;
    v.layerIndex = layerIdx;
    v.midiNote = midiNote;
    v.chokeGroup = z.chokeGroup;
    v.oneShot = z.oneShot;
    v.ageOrder = ++voiceAgeCounter;
    v.velocity = juce::jlimit (0.04f, 1.0f, velocity);
    v.envVal = 0.0f;
    v.envStage = 0;
    v.playingReverse = z.reverse;
    v.glideTotalSamples = 0;
    v.glideElapsedSamples = 0;
    v.legatoDipTotal = 0;
    v.legatoDipElapsed = 0;

    bool frrOn = (apvts.getRawParameterValue ("frr")->load() > 0.5f) && (numLayers == 1);
    bool fvlOn = apvts.getRawParameterValue ("fvl")->load() > 0.5f;
    bool pitchTrackOn = apvts.getRawParameterValue ("pitchTrack")->load() > 0.5f;
    float velRandAmt = apvts.getRawParameterValue ("velRand")->load();

    if (velRandAmt > 0.001f)
    {
        float delta = (rng.nextFloat() * 2.0f - 1.0f) * 0.25f * velRandAmt;
        v.velocity = juce::jlimit (0.04f, 1.0f, v.velocity + delta);
    }

    float rrPitchCents = 0.0f;
    float rrStartOffsetSamples = 0.0f;
    float rrGainMult = 1.0f;
    float rrExtraAttackMs = 0.0f;
    float rrCutMult = 1.0f;
    if (frrOn)
    {
        ++rrSeedCounter;
        double n = static_cast<double> (rrSeedCounter);
        float u1 = static_cast<float> (std::fmod (n * 0.61803398875 + rng.nextFloat() * 0.07, 1.0));
        float u2 = static_cast<float> (std::fmod (n * 0.56984029099 + rng.nextFloat() * 0.09, 1.0));
        float u3 = static_cast<float> (std::fmod (n * 0.43015970900 + rng.nextFloat() * 0.06, 1.0));

        rrPitchCents = (u1 * 2.0f - 1.0f) * 6.0f;
        if (std::abs (rrPitchCents - lastRrPitch) < 2.0f) rrPitchCents = -rrPitchCents;
        lastRrPitch = rrPitchCents;

        rrStartOffsetSamples = u2 * static_cast<float> (currentSampleRate * 0.0014);
        rrGainMult = 0.95f + u3 * 0.07f;
        rrExtraAttackMs = u2 * 0.9f;
        rrCutMult = 0.84f + u3 * 0.22f;
    }

    float fvlCutHz = 20000.0f;
    float fvlGainScale = 1.0f;
    float fvlExtraAttackMs = 0.0f;
    if (fvlOn)
    {
        float vNorm = juce::jlimit (0.0f, 1.0f, v.velocity);
        fvlCutHz = 500.0f * std::pow (40.0f, vNorm);
        fvlExtraAttackMs = std::pow (1.0f - vNorm, 2.0f) * 16.0f;
        float punchBoost = (vNorm > 0.80f) ? (1.0f + (vNorm - 0.80f) * 0.55f) : 1.0f;
        fvlGainScale = (0.35f + 0.65f * vNorm) * punchBoost;
    }

    float gain = std::pow (v.velocity, 1.08f) * fvlGainScale;

    float totalAttackSec = std::max (0.0001f, (z.attackMs + rrExtraAttackMs + fvlExtraAttackMs) * 0.001f);
    float decaySec = std::max (0.002f, z.decayMs * 0.001f);
    float releaseSec = std::max (0.002f, z.releaseMs * 0.001f);

    v.attackInc = 1.0f / std::max (1.0f, static_cast<float> (totalAttackSec * currentSampleRate));
    v.decayCoeff = std::exp (-4.6f / std::max (1.0f, static_cast<float> (decaySec * currentSampleRate)));
    v.sustainLvl = juce::jlimit (0.0f, 1.0f, z.sustainPct * 0.01f);
    v.releaseCoeff = std::exp (-6.2f / std::max (1.0f, static_cast<float> (releaseSec * currentSampleRate)));
    v.fastKillDec = 1.0f / std::max (1.0f, static_cast<float> (0.004 * currentSampleRate));

    double noteSemiOffset = pitchTrackOn ? static_cast<double> (midiNote - z.rootKey) : 0.0;
    double semitones = noteSemiOffset + static_cast<double> (z.pitchSemi) + (rrPitchCents / 100.0);
    v.baseSampleRateRatio = layer.sampleRate / currentSampleRate;
    v.currentSemi = semitones;
    v.startSemi = semitones;
    v.targetSemi = semitones;

    int startSamp = static_cast<int> (z.startNorm * totalSamps);
    int endSamp = std::max (startSamp + 2, static_cast<int> (z.endNorm * totalSamps));
    v.currentPos = z.reverse ? static_cast<double> (std::max (startSamp, endSamp - 2))
                             : (static_cast<double> (startSamp) + rrStartOffsetSamples);

    float panNorm = juce::jlimit (-1.0f, 1.0f, z.pan50 / 50.0f);
    float panAngle = (panNorm + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    float finalGain = gain * rrGainMult * vol100ToLinearGain (z.volumeDb100);
    v.gainL = finalGain * std::cos (panAngle) * 1.41421356f;
    v.gainR = finalGain * std::sin (panAngle) * 1.41421356f;

    v.filterL.reset();
    v.filterR.reset();
    v.baseCutoffHz = juce::jlimit (80.0f, 20000.0f, std::min (z.cutoffHz, fvlCutHz) * rrCutMult);
    v.baseResQ = z.resQ;
    v.useFilter = (v.baseCutoffHz < 19200.0f || v.baseResQ > 0.80f);
    if (v.useFilter)
    {
        v.filterL.setParams (v.baseCutoffHz, v.baseResQ, currentSampleRate);
        v.filterR.setParams (v.baseCutoffHz, v.baseResQ, currentSampleRate);
    }
}

void RythmeAudioProcessor::glideExistingLegatoVoices (int newMidiNote, float velocity)
{
    bool pitchTrackOn = apvts.getRawParameterValue ("pitchTrack")->load() > 0.5f;
    float baseGlideMs = apvts.getRawParameterValue ("glideMs")->load();

    float velFactor = 1.35f - juce::jlimit (0.0f, 1.0f, velocity) * 0.70f;
    float effGlideSec = juce::jlimit (0.002f, 0.500f, (baseGlideMs * velFactor) * 0.001f);
    int glideSamps = std::max (32, static_cast<int> (effGlideSec * currentSampleRate));
    int dipSamps = std::max (64, static_cast<int> (0.018 * currentSampleRate));

    for (auto& v : voices)
    {
        if (! v.active || v.isPreview || v.envStage >= 3) continue;
        if (v.zoneIndex < 0 || v.zoneIndex >= static_cast<int> (zones.size())) continue;
        const auto& z = zones[static_cast<size_t> (v.zoneIndex)];

        v.midiNote = newMidiNote;
        v.startSemi = v.currentSemi;
        double noteSemiOffset = pitchTrackOn ? static_cast<double> (newMidiNote - z.rootKey) : 0.0;
        v.targetSemi = noteSemiOffset + static_cast<double> (z.pitchSemi);
        v.glideTotalSamples = glideSamps;
        v.glideElapsedSamples = 0;
        v.legatoDipTotal = dipSamps;
        v.legatoDipElapsed = 0;
    }
}

void RythmeAudioProcessor::triggerNoteInternal (int midiNote, float velocity, bool isPreviewVoice)
{
    lastPlayedNote.store (midiNote);
    if (midiNote >= 0 && midiNote < 128)
        activeNotesUi[static_cast<size_t> (midiNote)].store (1);

    if (! isPreviewVoice && isKeyswitchNote (midiNote))
    {
        activeKeyswitchNote.store (midiNote);
        for (int i = 0; i < static_cast<int> (groups.size()); ++i)
            if (groups[static_cast<size_t> (i)].keyswitchNote == midiNote)
            {
                selectedGroupIndex = i;
                break;
            }
        return;
    }

    int playMode = static_cast<int> (apvts.getRawParameterValue ("playMode")->load());

    if (! isPreviewVoice && playMode == 2)
    {
        bool hadHeld = ! legatoNoteStack.empty();
        legatoNoteStack.erase (std::remove_if (legatoNoteStack.begin(), legatoNoteStack.end(),
            [midiNote] (const HeldNoteEntry& e) { return e.note == midiNote; }), legatoNoteStack.end());
        legatoNoteStack.push_back ({ midiNote, velocity });

        bool hasLiveVoice = false;
        for (const auto& v : voices)
            if (v.active && ! v.isPreview && v.envStage < 3) { hasLiveVoice = true; break; }

        if (hadHeld && hasLiveVoice)
        {
            glideExistingLegatoVoices (midiNote, velocity);
            return;
        }
    }

    int velInt = juce::jlimit (0, 127, static_cast<int> (std::round (velocity * 127.0f)));
    for (int i = 0; i < static_cast<int> (zones.size()); ++i)
    {
        const auto& z = zones[static_cast<size_t> (i)];
        if (midiNote >= z.lowKey && midiNote <= z.highKey && velInt >= z.lowVel && velInt <= z.highVel)
            startVoice (i, midiNote, velocity, isPreviewVoice);
    }
}

void RythmeAudioProcessor::releaseNoteInternal (int midiNote)
{
    if (midiNote >= 0 && midiNote < 128)
        activeNotesUi[static_cast<size_t> (midiNote)].store (0);
    if (lastPlayedNote.load() == midiNote)
        lastPlayedNote.store (-1);

    int playMode = static_cast<int> (apvts.getRawParameterValue ("playMode")->load());

    if (playMode == 2 && ! legatoNoteStack.empty())
    {
        bool wasTop = (legatoNoteStack.back().note == midiNote);
        legatoNoteStack.erase (std::remove_if (legatoNoteStack.begin(), legatoNoteStack.end(),
            [midiNote] (const HeldNoteEntry& e) { return e.note == midiNote; }), legatoNoteStack.end());

        if (! legatoNoteStack.empty())
        {
            if (wasTop)
            {
                const auto& prev = legatoNoteStack.back();
                lastPlayedNote.store (prev.note);
                glideExistingLegatoVoices (prev.note, prev.vel);
            }
            return;
        }
    }

    if (sustainPedalDown && midiNote >= 0 && midiNote < 128)
    {
        sustainedNotes[static_cast<size_t> (midiNote)] = true;
        return;
    }

    for (auto& v : voices)
        if (v.active && ! v.isPreview && ! v.oneShot && v.midiNote == midiNote && v.envStage < 3)
            v.envStage = 3;
}

void RythmeAudioProcessor::triggerNote (int midiNote, float velocity)
{
    const juce::ScopedLock sl (zoneLock);
    triggerNoteInternal (midiNote, velocity, false);
}

void RythmeAudioProcessor::releaseNote (int midiNote)
{
    const juce::ScopedLock sl (zoneLock);
    releaseNoteInternal (midiNote);
}

void RythmeAudioProcessor::toggleZonePreview (int zoneIdx)
{
    const juce::ScopedLock sl (zoneLock);
    if (previewActive.load())
    {
        for (auto& v : voices)
            if (v.active && v.isPreview)
                v.envStage = 4;
        if (previewNote >= 0 && previewNote < 128)
            activeNotesUi[static_cast<size_t> (previewNote)].store (0);
        previewActive.store (false);
        previewNote = -1;
        return;
    }

    if (zoneIdx < 0 || zoneIdx >= static_cast<int> (zones.size()))
        return;
    const auto& z = zones[static_cast<size_t> (zoneIdx)];
    if (z.layers.empty()) return;

    previewNote = z.rootKey;
    previewActive.store (true);
    if (previewNote >= 0 && previewNote < 128)
        activeNotesUi[static_cast<size_t> (previewNote)].store (1);
    startVoice (zoneIdx, z.rootKey, 0.90f, true);
}

void RythmeAudioProcessor::stopAllVoicesSmooth()
{
    const juce::ScopedLock sl (zoneLock);
    for (auto& v : voices)
        if (v.active)
            v.envStage = 4;
    heldNotesVel.fill (0.0f);
    sustainedNotes.fill (false);
    sustainPedalDown = false;
    legatoNoteStack.clear();
    previewActive.store (false);
    previewNote = -1;
    for (auto& a : activeNotesUi) a.store (0);
    lastPlayedNote.store (-1);
}

bool RythmeAudioProcessor::hasAnyHeldNote() const
{
    for (float v : heldNotesVel)
        if (v > 0.0f) return true;
    return false;
}

void RythmeAudioProcessor::uiNoteOn (int midiNote, float velocity)
{
    const juce::ScopedLock sl (zoneLock);
    if (isKeyswitchNote (midiNote))
    {
        activeKeyswitchNote.store (midiNote);
        for (int i = 0; i < static_cast<int> (groups.size()); ++i)
            if (groups[static_cast<size_t> (i)].keyswitchNote == midiNote)
            {
                selectedGroupIndex = i;
                break;
            }
        return;
    }

    bool seqLoopOn = (apvts.getRawParameterValue ("seqLoop")->load() > 0.5f);
    bool wasIdle = ! hasAnyHeldNote() && ! internalPlaying.load();

    if (midiNote >= 0 && midiNote < 128)
        heldNotesVel[static_cast<size_t> (midiNote)] = velocity;

    if (! seqLoopOn)
    {
        triggerNoteInternal (midiNote, velocity, false);
    }
    else
    {
        if (wasIdle) { seqBarPhase = 0.0; seqPlayheadNorm.store (0.0f); }
        lastPlayedNote.store (midiNote);
        if (midiNote >= 0 && midiNote < 128)
            activeNotesUi[static_cast<size_t> (midiNote)].store (1);
        int velInt = juce::jlimit (0, 127, static_cast<int> (std::round (velocity * 127.0f)));
        for (int i = 0; i < static_cast<int> (zones.size()); ++i)
        {
            const auto& z = zones[static_cast<size_t> (i)];
            if (midiNote >= z.lowKey && midiNote <= z.highKey && velInt >= z.lowVel && velInt <= z.highVel)
                for (const auto& ev : z.seqEvents)
                    if (ev.position < 0.004f)
                        startVoice (i, midiNote, velocity * ev.velocity, false);
        }
    }
}

void RythmeAudioProcessor::uiNoteOff (int midiNote)
{
    const juce::ScopedLock sl (zoneLock);
    if (midiNote >= 0 && midiNote < 128)
        heldNotesVel[static_cast<size_t> (midiNote)] = 0.0f;
    releaseNoteInternal (midiNote);
}

void RythmeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto blockStartTicks = juce::Time::getHighResolutionTicks();

    buffer.clear();
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    const juce::ScopedTryLock tryLock (zoneLock);
    if (! tryLock.isLocked())
        return;

    bool seqLoopOn = (apvts.getRawParameterValue ("seqLoop")->load() > 0.5f);
    bool hostSyncOn = apvts.getRawParameterValue ("hostSync")->load() > 0.5f;
    double bpm = static_cast<double> (apvts.getRawParameterValue ("internalBpm")->load());
    bool dawPlaying = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (hostSyncOn)
            {
                if (auto b = pos->getBpm()) bpm = *b;
                dawPlaying = pos->getIsPlaying();
            }
        }
    }

    float shuffleAmt = apvts.getRawParameterValue ("shuffle")->load();
    float timeRandAmt = apvts.getRawParameterValue ("timeRand")->load();

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int nn = msg.getNoteNumber();
            float vel = msg.getFloatVelocity();
            if (isKeyswitchNote (nn))
            {
                activeKeyswitchNote.store (nn);
                for (int i = 0; i < static_cast<int> (groups.size()); ++i)
                    if (groups[static_cast<size_t> (i)].keyswitchNote == nn) { selectedGroupIndex = i; break; }
            }
            else
            {
                bool wasIdle = ! hasAnyHeldNote() && ! internalPlaying.load();
                if (nn >= 0 && nn < 128) heldNotesVel[static_cast<size_t> (nn)] = vel;
                if (! seqLoopOn)
                    triggerNoteInternal (nn, vel, false);
                else
                {
                    if (wasIdle) { seqBarPhase = 0.0; seqPlayheadNorm.store (0.0f); }
                    lastPlayedNote.store (nn);
                    if (nn >= 0 && nn < 128) activeNotesUi[static_cast<size_t> (nn)].store (1);
                    int velInt = juce::jlimit (0, 127, static_cast<int> (std::round (vel * 127.0f)));
                    for (int i = 0; i < static_cast<int> (zones.size()); ++i)
                    {
                        const auto& z = zones[static_cast<size_t> (i)];
                        if (nn >= z.lowKey && nn <= z.highKey && velInt >= z.lowVel && velInt <= z.highVel)
                            for (const auto& ev : z.seqEvents)
                                if (ev.position < 0.004f)
                                    startVoice (i, nn, vel * ev.velocity, false);
                    }
                }
            }
        }
        else if (msg.isNoteOff())
        {
            int nn = msg.getNoteNumber();
            if (nn >= 0 && nn < 128) heldNotesVel[static_cast<size_t> (nn)] = 0.0f;
            releaseNoteInternal (nn);
        }
        else if (msg.isPitchWheel())
        {
            float pw = (msg.getPitchWheelValue() - 8192) / 8192.0f;
            pitchWheelNorm.store (juce::jlimit (-1.0f, 1.0f, pw));
        }
        else if (msg.isController())
        {
            int cc = msg.getControllerNumber();
            int val = msg.getControllerValue();
            if (cc == 1)
                modWheelNorm.store (juce::jlimit (0.0f, 1.0f, val / 127.0f));
            else if (cc == 11)
                expressionNorm.store (juce::jlimit (0.0f, 1.0f, val / 127.0f));
            else if (cc == 64)
            {
                bool down = (val >= 64);
                if (sustainPedalDown && ! down)
                {
                    sustainPedalDown = false;
                    for (int n = 0; n < 128; ++n)
                    {
                        if (sustainedNotes[static_cast<size_t> (n)])
                        {
                            sustainedNotes[static_cast<size_t> (n)] = false;
                            if (heldNotesVel[static_cast<size_t> (n)] <= 0.0f)
                                releaseNoteInternal (n);
                        }
                    }
                }
                else
                {
                    sustainPedalDown = down;
                }
            }
            else if (cc == 120 || cc == 123)
            {
                for (auto& v : voices)
                    if (v.active) v.envStage = (cc == 120 ? 4 : 3);
                heldNotesVel.fill (0.0f);
                sustainedNotes.fill (false);
                sustainPedalDown = false;
                legatoNoteStack.clear();
                for (auto& a : activeNotesUi) a.store (0);
                lastPlayedNote.store (-1);
            }
        }
    }

    bool transportAllPlay = seqLoopOn && (internalPlaying.load() || (hostSyncOn && dawPlaying));
    bool anyMidiHeld = seqLoopOn && hasAnyHeldNote();
    bool shouldRunSeq = transportAllPlay || anyMidiHeld;

    if (transportAllPlay && ! wasTransportPlaying)
    {
        seqBarPhase = 0.0;
        seqPlayheadNorm.store (0.0f);
        for (int zIdx = 0; zIdx < static_cast<int> (zones.size()); ++zIdx)
            for (const auto& ev : zones[static_cast<size_t> (zIdx)].seqEvents)
                if (ev.position < 0.004f)
                    startVoice (zIdx, zones[static_cast<size_t> (zIdx)].rootKey, 0.9f * ev.velocity, false);
    }
    wasTransportPlaying = transportAllPlay;

    if (shouldRunSeq)
    {
        double samplesPerBar = (60.0 / std::max (20.0, bpm)) * 4.0 * currentSampleRate;
        double prevPhase = seqBarPhase;
        double deltaPhase = static_cast<double> (numSamples) / std::max (1.0, samplesPerBar);
        seqBarPhase += deltaPhase;
        bool wrapped = false;
        if (seqBarPhase >= 1.0)
        {
            seqBarPhase -= 1.0;
            wrapped = true;
        }
        seqPlayheadNorm.store (static_cast<float> (seqBarPhase));

        int divs = getGridDivisions();
        for (int zIdx = 0; zIdx < static_cast<int> (zones.size()); ++zIdx)
        {
            const auto& z = zones[static_cast<size_t> (zIdx)];
            if (z.layers.empty()) continue;

            int triggerMidi = -1;
            float triggerVel = 0.0f;

            if (transportAllPlay)
            {
                triggerMidi = z.rootKey;
                triggerVel = 0.9f;
            }
            else
            {
                for (int k = z.lowKey; k <= z.highKey; ++k)
                {
                    if (k >= 0 && k < 128 && heldNotesVel[static_cast<size_t> (k)] > 0.0f)
                    {
                        int vInt = juce::jlimit (0, 127, static_cast<int> (std::round (heldNotesVel[static_cast<size_t> (k)] * 127.0f)));
                        if (vInt >= z.lowVel && vInt <= z.highVel)
                        {
                            triggerMidi = k;
                            triggerVel = heldNotesVel[static_cast<size_t> (k)];
                            break;
                        }
                    }
                }
            }

            if (triggerMidi < 0) continue;

            for (const auto& ev : z.seqEvents)
            {
                double evPos = static_cast<double> (ev.position);
                int subIdx = static_cast<int> (std::round (evPos * divs));
                if (subIdx % 2 == 1 && shuffleAmt > 0.001f)
                    evPos += (shuffleAmt * 0.35) / static_cast<double> (divs);
                if (timeRandAmt > 0.001f)
                    evPos += (rng.nextFloat() * 2.0f - 1.0f) * 0.010 * timeRandAmt;
                evPos = std::fmod (evPos + 1.0, 1.0);

                bool triggered = (! wrapped && evPos > prevPhase && evPos <= seqBarPhase)
                              || (wrapped && (evPos > prevPhase || evPos <= seqBarPhase));
                if (triggered)
                    startVoice (zIdx, triggerMidi, triggerVel * ev.velocity, false);
            }
        }
    }

    float pwNorm = pitchWheelNorm.load();
    float mwNorm = modWheelNorm.load();
    float exprGain = 0.15f + 0.85f * expressionNorm.load();

    vibratoPhase += (5.4 * juce::MathConstants<double>::twoPi * numSamples) / currentSampleRate;
    if (vibratoPhase > juce::MathConstants<double>::twoPi)
        vibratoPhase -= juce::MathConstants<double>::twoPi;

    double vibSemis = (mwNorm > 0.02f) ? (std::sin (vibratoPhase) * 0.22 * static_cast<double> (mwNorm)) : 0.0;
    double globalBendRatio = std::pow (2.0, (static_cast<double> (pwNorm) * 2.0 + vibSemis) / 12.0);
    float modBrightScale = 1.0f + mwNorm * 0.35f;

    if (groupScratchBuf.getNumSamples() < numSamples)
        groupScratchBuf.setSize (2, numSamples, false, false, true);

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    bool anySolo = hasAnySoloedGroup();
    bool bypassAllFx = fxBypass.load();
    bool anyPreviewRunning = false;
    float selPlayhead = -1.0f;
    int sleepLimit = static_cast<int> (currentSampleRate * 0.20);

    for (int gIdx = 0; gIdx < static_cast<int> (groups.size()); ++gIdx)
    {
        auto& grp = groups[static_cast<size_t> (gIdx)];
        if (grp.muted || (anySolo && ! grp.soloed)) continue;

        bool hasActiveVoice = false;
        for (const auto& v : voices)
            if (v.active && v.zoneIndex >= 0 && v.zoneIndex < static_cast<int> (zones.size())
                && zones[static_cast<size_t> (v.zoneIndex)].groupIndex == gIdx)
            { hasActiveVoice = true; break; }

        bool hasTailFx = false;
        if (! bypassAllFx)
            for (const auto& slot : grp.fxSlots)
                if (slot.enabled && (slot.type == SimpleFxType::Delay || slot.type == SimpleFxType::Reverb))
                { hasTailFx = true; break; }

        if (! hasActiveVoice && (! hasTailFx || grp.silentSamples >= sleepLimit))
            continue;

        groupScratchBuf.clear (0, numSamples);
        float* gL = groupScratchBuf.getWritePointer (0);
        float* gR = groupScratchBuf.getWritePointer (1);

        if (hasActiveVoice)
        {
            for (auto& v : voices)
            {
                if (! v.active || v.zoneIndex < 0 || v.zoneIndex >= static_cast<int> (zones.size())) continue;
                const auto& z = zones[static_cast<size_t> (v.zoneIndex)];
                if (z.groupIndex != gIdx) continue;
                if (v.layerIndex < 0 || v.layerIndex >= static_cast<int> (z.layers.size())) continue;

                const auto& layer = z.layers[static_cast<size_t> (v.layerIndex)];
                int totalSamps = layer.buffer.getNumSamples();
                if (totalSamps < 4) continue;

                int startSample = juce::jlimit (0, totalSamps - 3, static_cast<int> (z.startNorm * totalSamps));
                int endSample   = juce::jlimit (startSample + 2, totalSamps - 1, static_cast<int> (z.endNorm * totalSamps));
                int lStartSample = juce::jlimit (startSample, endSample - 2, static_cast<int> (z.loopStartNorm * totalSamps));
                int lEndSample   = juce::jlimit (lStartSample + 2, endSample, static_cast<int> (z.loopEndNorm * totalSamps));
                int activeSpan   = std::max (2, endSample - startSample);
                int fadeInSamps  = static_cast<int> (z.fadeInNorm * activeSpan);
                int fadeOutSamps = static_cast<int> (z.fadeOutNorm * activeSpan);
                double loopLen   = static_cast<double> (std::max (2, lEndSample - lStartSample));
                int maxXfade     = std::max (0, static_cast<int> (loopLen * 0.45));
                int xfadeSamps   = juce::jlimit (0, maxXfade, static_cast<int> (z.loopCrossfade * loopLen));
                bool canXfade    = (z.sampleLoop && xfadeSamps > 6);

                bool isGliding = (v.glideTotalSamples > 0 && v.glideElapsedSamples < v.glideTotalSamples);
                double blockPitchRatio = v.baseSampleRateRatio * std::pow (2.0, v.currentSemi / 12.0) * globalBendRatio;

                bool doFilter = v.useFilter || (mwNorm > 0.05f && v.baseCutoffHz < 19500.0f);
                if (doFilter)
                {
                    float effCut = juce::jlimit (80.0f, 20000.0f, v.baseCutoffHz * modBrightScale);
                    v.filterL.setParams (effCut, v.baseResQ, currentSampleRate);
                    v.filterR.setParams (effCut, v.baseResQ, currentSampleRate);
                }

                const float* srcL = layer.buffer.getReadPointer (0);
                const float* srcR = layer.buffer.getNumChannels() > 1 ? layer.buffer.getReadPointer (1) : srcL;

                for (int i = 0; i < numSamples; ++i)
                {
                    if (isGliding && (i & 7) == 0)
                    {
                        v.glideElapsedSamples += 8;
                        double u = juce::jlimit (0.0, 1.0, static_cast<double> (v.glideElapsedSamples) / static_cast<double> (v.glideTotalSamples));
                        double sCurve = u * u * (3.0 - 2.0 * u);
                        v.currentSemi = v.startSemi + (v.targetSemi - v.startSemi) * sCurve;
                        blockPitchRatio = v.baseSampleRateRatio * std::pow (2.0, v.currentSemi / 12.0) * globalBendRatio;
                        if (v.glideElapsedSamples >= v.glideTotalSamples)
                        {
                            v.currentSemi = v.targetSemi;
                            isGliding = false;
                        }
                    }

                    if (z.sampleLoop)
                    {
                        if (! v.playingReverse && v.currentPos >= static_cast<double> (lEndSample))
                            v.currentPos -= loopLen;
                        else if (v.playingReverse && v.currentPos <= static_cast<double> (lStartSample))
                            v.currentPos += loopLen;
                    }

                    int posInt = static_cast<int> (v.currentPos);
                    if (posInt < startSample || posInt >= endSample || posInt >= totalSamps - 1)
                    {
                        v.active = false;
                        break;
                    }

                    float sL = hermiteSample (srcL, totalSamps, v.currentPos);
                    float sR = (srcR == srcL) ? sL : hermiteSample (srcR, totalSamps, v.currentPos);

                    if (canXfade)
                    {
                        if (! v.playingReverse)
                        {
                            double xStart = static_cast<double> (lEndSample - xfadeSamps);
                            if (v.currentPos >= xStart)
                            {
                                float alpha = juce::jlimit (0.0f, 1.0f, static_cast<float> ((v.currentPos - xStart) / static_cast<double> (xfadeSamps)));
                                double altPos = v.currentPos - loopLen;
                                if (altPos < static_cast<double> (startSample))
                                    altPos = static_cast<double> (lStartSample) + (v.currentPos - xStart);
                                altPos = juce::jlimit (0.0, static_cast<double> (totalSamps - 2), altPos);

                                float gOut = std::cos (alpha * juce::MathConstants<float>::halfPi);
                                float gIn  = std::sin (alpha * juce::MathConstants<float>::halfPi);
                                float aL = hermiteSample (srcL, totalSamps, altPos);
                                float aR = (srcR == srcL) ? aL : hermiteSample (srcR, totalSamps, altPos);
                                sL = sL * gOut + aL * gIn;
                                sR = sR * gOut + aR * gIn;
                            }
                        }
                        else
                        {
                            double xEnd = static_cast<double> (lStartSample + xfadeSamps);
                            if (v.currentPos <= xEnd)
                            {
                                float alpha = juce::jlimit (0.0f, 1.0f, static_cast<float> ((xEnd - v.currentPos) / static_cast<double> (xfadeSamps)));
                                double altPos = v.currentPos + loopLen;
                                if (altPos >= static_cast<double> (endSample - 1))
                                    altPos = static_cast<double> (lEndSample) - (xEnd - v.currentPos);
                                altPos = juce::jlimit (0.0, static_cast<double> (totalSamps - 2), altPos);

                                float gOut = std::cos (alpha * juce::MathConstants<float>::halfPi);
                                float gIn  = std::sin (alpha * juce::MathConstants<float>::halfPi);
                                float aL = hermiteSample (srcL, totalSamps, altPos);
                                float aR = (srcR == srcL) ? aL : hermiteSample (srcR, totalSamps, altPos);
                                sL = sL * gOut + aL * gIn;
                                sR = sR * gOut + aR * gIn;
                            }
                        }
                    }

                    float edgeGain = 1.0f;
                    if (fadeInSamps > 4 && (posInt - startSample) < fadeInSamps)
                    {
                        float t = static_cast<float> (posInt - startSample) / static_cast<float> (fadeInSamps);
                        edgeGain *= t * t;
                    }
                    if (fadeOutSamps > 4 && (endSample - posInt) < fadeOutSamps)
                    {
                        float t = static_cast<float> (endSample - posInt) / static_cast<float> (fadeOutSamps);
                        edgeGain *= t * t;
                    }

                    float dipGain = 1.0f;
                    if (v.legatoDipElapsed < v.legatoDipTotal)
                    {
                        float dNorm = static_cast<float> (v.legatoDipElapsed) / static_cast<float> (std::max (1, v.legatoDipTotal));
                        dipGain = 1.0f - 0.14f * std::sin (dNorm * juce::MathConstants<float>::pi);
                        ++v.legatoDipElapsed;
                    }

                    if (v.envStage == 0)
                    {
                        v.envVal += v.attackInc;
                        if (v.envVal >= 1.0f) { v.envVal = 1.0f; v.envStage = 1; }
                    }
                    else if (v.envStage == 1)
                    {
                        v.envVal = v.sustainLvl + (v.envVal - v.sustainLvl) * v.decayCoeff;
                        if (std::abs (v.envVal - v.sustainLvl) < 0.0008f) { v.envVal = v.sustainLvl; v.envStage = 2; }
                    }
                    else if (v.envStage == 3)
                    {
                        v.envVal *= v.releaseCoeff;
                        if (v.envVal <= 0.00025f) { v.envVal = 0.0f; v.active = false; break; }
                    }
                    else if (v.envStage == 4)
                    {
                        v.envVal -= v.fastKillDec;
                        if (v.envVal <= 0.0001f) { v.envVal = 0.0f; v.active = false; break; }
                    }

                    if (doFilter)
                    {
                        sL = v.filterL.processLP (sL);
                        sR = v.filterR.processLP (sR);
                    }

                    float totalAmp = v.envVal * edgeGain * dipGain * exprGain;
                    gL[i] += sL * totalAmp * v.gainL;
                    gR[i] += sR * totalAmp * v.gainR;

                    v.currentPos += (v.playingReverse ? -blockPitchRatio : blockPitchRatio);
                }

                if (v.active)
                {
                    if (v.isPreview) anyPreviewRunning = true;
                    if (v.zoneIndex == selectedZoneIndex)
                        selPlayhead = juce::jlimit (0.0f, 1.0f, static_cast<float> (v.currentPos / static_cast<double> (totalSamps)));
                }
            }
        }

        if (! bypassAllFx)
            for (auto& slot : grp.fxSlots)
                slot.processStereo (gL, gR, numSamples, currentSampleRate);

        float gPanNorm = juce::jlimit (-1.0f, 1.0f, grp.pan50 / 50.0f);
        float panAngle = (gPanNorm + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        float gLin = vol100ToLinearGain (grp.volumeDb100);
        float gGL = gLin * std::cos (panAngle) * 1.41421356f;
        float gGR = gLin * std::sin (panAngle) * 1.41421356f;

        float grpPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float sL = gL[i] * gGL;
            float sR = gR[i] * gGR;
            outL[i] += sL;
            outR[i] += sR;
            grpPeak = std::max (grpPeak, std::max (std::abs (sL), std::abs (sR)));
        }

        if (hasActiveVoice || grpPeak > 0.00012f)
            grp.silentSamples = 0;
        else
            grp.silentSamples = std::min (sleepLimit + 1024, grp.silentSamples + numSamples);
    }

    if (previewActive.load() && ! anyPreviewRunning)
    {
        previewActive.store (false);
        if (previewNote >= 0 && previewNote < 128)
            activeNotesUi[static_cast<size_t> (previewNote)].store (0);
        previewNote = -1;
    }
    selectedZonePlayheadNorm.store (selPlayhead);

    float mGain = juce::Decibels::decibelsToGain (masterGainDb.load(), -36.0f);
    float peakL = 0.0f, peakR = 0.0f;
    int fIdx = fftWriteIdx.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] *= mGain;
        outR[i] *= mGain;
        peakL = std::max (peakL, std::abs (outL[i]));
        peakR = std::max (peakR, std::abs (outR[i]));
        fftRing[static_cast<size_t> (fIdx)] = 0.5f * (outL[i] + outR[i]);
        fIdx = (fIdx + 1) % fftRingSize;
    }
    fftWriteIdx.store (fIdx, std::memory_order_release);

    float blockPeak = std::max (peakL, peakR);
    outputMeterL.store (std::max (juce::jlimit (0.0f, 1.0f, peakL), outputMeterL.load() * 0.85f));
    outputMeterR.store (std::max (juce::jlimit (0.0f, 1.0f, peakR), outputMeterR.load() * 0.85f));

    {
        const juce::SpinLock::ScopedLockType ssl (scopeLock);
        if (blockPeak > 0.0004f)
        {
            silentSamplesHold = 0;
            int step = std::max (1, numSamples / 4);
            for (int i = 0; i < numSamples; i += step)
            {
                float mono = 0.5f * (outL[i] + outR[i]);
                scopeRing[static_cast<size_t> (scopeWriteIdx)] = juce::jlimit (-1.0f, 1.0f, mono);
                scopeWriteIdx = (scopeWriteIdx + 1) % scopeHistorySize;
            }
        }
        else
        {
            silentSamplesHold += numSamples;
            if (silentSamplesHold >= static_cast<int> (currentSampleRate * 0.14))
                scopeRing.fill (0.0f);
        }
    }

    const auto elapsedTicks = juce::Time::getHighResolutionTicks() - blockStartTicks;
    double elapsedSec = juce::Time::highResolutionTicksToSeconds (elapsedTicks);
    double blockSec = static_cast<double> (numSamples) / std::max (22050.0, currentSampleRate);
    float instPct = static_cast<float> (juce::jlimit (0.0, 99.9, (elapsedSec / std::max (1e-6, blockSec)) * 100.0));
    float prevCpu = cpuUsagePercent.load();
    cpuUsagePercent.store (prevCpu * 0.90f + instPct * 0.10f);
}

std::unique_ptr<juce::XmlElement> RythmeAudioProcessor::createFullStateXml (bool includeUiLayout)
{
    auto xml = std::make_unique<juce::XmlElement> ("KINETIK_PRESET");
    xml->setAttribute ("formatVersion", 3);
    xml->setAttribute ("presetName", currentPresetName);
    xml->setAttribute ("libraryName", libraryNameText);
    xml->setAttribute ("brandLetters", brandLetters);
    xml->setAttribute ("pluginTitleText", pluginTitleText);
    xml->setAttribute ("masterGainDb", static_cast<double> (masterGainDb.load()));
    xml->setAttribute ("fxBypass", fxBypass.load() ? 1 : 0);
    xml->setAttribute ("selectGroupOnly", selectGroupOnly.load() ? 1 : 0);
    xml->setAttribute ("uiThemeId", uiThemeId);
    xml->setAttribute ("logoStyleId", logoStyleId);
    xml->setAttribute ("logoColourIdx", logoColourIdx);
    xml->setAttribute ("knobStyleId", knobStyleId);
    xml->setAttribute ("knobColourIdx", knobColourIdx);
    xml->setAttribute ("faderStyleId", faderStyleId);
    xml->setAttribute ("faderColourIdx", faderColourIdx);
    xml->setAttribute ("switchStyleId", switchStyleId);
    xml->setAttribute ("switchColourIdx", switchColourIdx);

    auto state = apvts.copyState();
    if (auto apvtsXml = state.createXml())
    {
        auto* apvtsWrap = xml->createNewChildElement ("APVTS");
        apvtsWrap->addChildElement (apvtsXml.release());
    }

    const juce::ScopedLock sl (zoneLock);
    auto* groupsXml = xml->createNewChildElement ("GROUPS");
    for (const auto& g : groups)
    {
        auto* gx = groupsXml->createNewChildElement ("GROUP");
        gx->setAttribute ("name", g.name);
        gx->setAttribute ("muted", g.muted ? 1 : 0);
        gx->setAttribute ("soloed", g.soloed ? 1 : 0);
        gx->setAttribute ("ksNote", g.keyswitchNote);
        gx->setAttribute ("vol100", g.volumeDb100);
        gx->setAttribute ("pan50", g.pan50);
        for (int s = 0; s < SampleGroup::numFxSlots; ++s)
        {
            const auto& slot = g.fxSlots[static_cast<size_t> (s)];
            auto* sx = gx->createNewChildElement ("FX");
            sx->setAttribute ("idx", s);
            sx->setAttribute ("type", static_cast<int> (slot.type));
            sx->setAttribute ("en", slot.enabled ? 1 : 0);
            sx->setAttribute ("p1", slot.p1); sx->setAttribute ("p2", slot.p2);
            sx->setAttribute ("p3", slot.p3); sx->setAttribute ("p4", slot.p4); sx->setAttribute ("p5", slot.p5);
        }
    }

    auto* zonesXml = xml->createNewChildElement ("ZONES");
    for (const auto& z : zones)
    {
        auto* zx = zonesXml->createNewChildElement ("ZONE");
        zx->setAttribute ("name", z.name);
        zx->setAttribute ("groupIdx", z.groupIndex);
        zx->setAttribute ("lowKey", z.lowKey);
        zx->setAttribute ("highKey", z.highKey);
        zx->setAttribute ("rootKey", z.rootKey);
        zx->setAttribute ("lowVel", z.lowVel);
        zx->setAttribute ("highVel", z.highVel);
        zx->setAttribute ("colIdx", z.colourIndex);
        zx->setAttribute ("realRR", z.realRREnabled ? 1 : 0);
        zx->setAttribute ("selLayer", z.selectedLayerIndex);
        zx->setAttribute ("oneShot", z.oneShot ? 1 : 0);
        zx->setAttribute ("chokeGrp", z.chokeGroup);

        zx->setAttribute ("start", z.startNorm);
        zx->setAttribute ("end", z.endNorm);
        zx->setAttribute ("lStart", z.loopStartNorm);
        zx->setAttribute ("lEnd", z.loopEndNorm);
        zx->setAttribute ("sLoop", z.sampleLoop ? 1 : 0);
        zx->setAttribute ("autoZLoop", z.autoLoopZeroCross ? 1 : 0);
        zx->setAttribute ("lXfade", z.loopCrossfade);
        zx->setAttribute ("fIn", z.fadeInNorm);
        zx->setAttribute ("fOut", z.fadeOutNorm);
        zx->setAttribute ("rev", z.reverse ? 1 : 0);
        zx->setAttribute ("pitch", z.pitchSemi);
        zx->setAttribute ("pan50", z.pan50);
        zx->setAttribute ("vol100", z.volumeDb100);
        zx->setAttribute ("atkMs", z.attackMs);
        zx->setAttribute ("decMs", z.decayMs);
        zx->setAttribute ("susPct", z.sustainPct);
        zx->setAttribute ("relMs", z.releaseMs);
        zx->setAttribute ("cut", z.cutoffHz);
        zx->setAttribute ("res", z.resQ);

        auto* layersXml = zx->createNewChildElement ("LAYERS");
        for (const auto& l : z.layers)
            layersXml->createNewChildElement ("LAYER")->setAttribute ("file", l.file.getFullPathName());

        auto* seqXml = zx->createNewChildElement ("SEQ_EVENTS");
        for (const auto& ev : z.seqEvents)
        {
            auto* ex = seqXml->createNewChildElement ("EV");
            ex->setAttribute ("pos", ev.position);
            ex->setAttribute ("vel", ev.velocity);
        }
    }

    if (includeUiLayout)
    {
        auto* lay = xml->createNewChildElement ("UI_LAYOUT");
        lay->setAttribute ("schemaVersion", 3);
        lay->setAttribute ("uiScale", static_cast<double> (uiScale));
        lay->setAttribute ("uiThemeId", uiThemeId);
        lay->setAttribute ("customizeGridStep", customizeGridStep);

        auto* mb = lay->createNewChildElement ("MAIN_BOUNDS");
        for (const auto& kv : mainCustomBounds)
        {
            auto* e = mb->createNewChildElement ("B");
            e->setAttribute ("k", kv.first);
            e->setAttribute ("x", kv.second.getX());
            e->setAttribute ("y", kv.second.getY());
            e->setAttribute ("w", kv.second.getWidth());
            e->setAttribute ("h", kv.second.getHeight());
        }
        auto* eb = lay->createNewChildElement ("EDIT_BOUNDS");
        for (const auto& kv : editCustomBounds)
        {
            auto* e = eb->createNewChildElement ("B");
            e->setAttribute ("k", kv.first);
            e->setAttribute ("x", kv.second.getX());
            e->setAttribute ("y", kv.second.getY());
            e->setAttribute ("w", kv.second.getWidth());
            e->setAttribute ("h", kv.second.getHeight());
        }
        auto* mh = lay->createNewChildElement ("MAIN_HIDDEN");
        for (const auto& k : mainHiddenKeys)
            mh->createNewChildElement ("H")->setAttribute ("k", k);
        auto* eh = lay->createNewChildElement ("EDIT_HIDDEN");
        for (const auto& k : editHiddenKeys)
            eh->createNewChildElement ("H")->setAttribute ("k", k);
    }

    return xml;
}

bool RythmeAudioProcessor::restoreFullStateFromXml (const juce::XmlElement& xml, bool restoreUiLayout)
{
    int fmtVer = xml.getIntAttribute ("formatVersion", 1);

    if (auto* apvtsWrap = xml.getChildByName ("APVTS"))
    {
        if (auto* pXml = apvtsWrap->getFirstChildElement())
        {
            auto vt = juce::ValueTree::fromXml (*pXml);
            if (vt.isValid() && vt.hasType (apvts.state.getType()))
                apvts.replaceState (vt);
        }
    }
    else if (xml.hasTagName (apvts.state.getType()))
    {
        juce::XmlElement cleanParams (xml);
        if (auto* g = cleanParams.getChildByName ("GROUPS")) cleanParams.removeChildElement (g, true);
        if (auto* z = cleanParams.getChildByName ("ZONES"))  cleanParams.removeChildElement (z, true);
        auto vt = juce::ValueTree::fromXml (cleanParams);
        if (vt.isValid())
            apvts.replaceState (vt);
    }

    currentPresetName = xml.getStringAttribute ("presetName", currentPresetName);
    libraryNameText   = xml.getStringAttribute ("libraryName", libraryNameText);
    brandLetters      = xml.getStringAttribute ("brandLetters", "N6");
    pluginTitleText   = xml.getStringAttribute ("pluginTitleText", "KINETIK");
    masterGainDb.store (juce::jlimit (-24.0f, 12.0f, static_cast<float> (xml.getDoubleAttribute ("masterGainDb", 0.0))));
    fxBypass.store (xml.getIntAttribute ("fxBypass", 0) != 0);
    selectGroupOnly.store (xml.getIntAttribute ("selectGroupOnly", 1) != 0);

    if (restoreUiLayout)
    {
        uiThemeId       = juce::jlimit (0, numUiThemes - 1, xml.getIntAttribute ("uiThemeId", uiThemeId));
        logoStyleId     = juce::jlimit (1, 4, xml.getIntAttribute ("logoStyleId", logoStyleId));
        logoColourIdx   = juce::jlimit (0, numPaletteColours - 1, xml.getIntAttribute ("logoColourIdx", logoColourIdx));
        knobStyleId     = juce::jlimit (1, 10, xml.getIntAttribute ("knobStyleId", knobStyleId));
        knobColourIdx   = juce::jlimit (0, numPaletteColours - 1, xml.getIntAttribute ("knobColourIdx", knobColourIdx));
        faderStyleId    = juce::jlimit (1, 10, xml.getIntAttribute ("faderStyleId", faderStyleId));
        faderColourIdx  = juce::jlimit (0, numPaletteColours - 1, xml.getIntAttribute ("faderColourIdx", faderColourIdx));
        switchStyleId   = juce::jlimit (1, 10, xml.getIntAttribute ("switchStyleId", switchStyleId));
        switchColourIdx = juce::jlimit (0, numPaletteColours - 1, xml.getIntAttribute ("switchColourIdx", switchColourIdx));

        if (auto* lay = xml.getChildByName ("UI_LAYOUT"))
        {
            int laySchema = lay->getIntAttribute ("schemaVersion", 1);
            uiScale           = juce::jlimit (0.5f, 1.5f, static_cast<float> (lay->getDoubleAttribute ("uiScale", uiScale)));
            uiThemeId         = juce::jlimit (0, numUiThemes - 1, lay->getIntAttribute ("uiThemeId", uiThemeId));
            customizeGridStep = juce::jlimit (0, 50, lay->getIntAttribute ("customizeGridStep", customizeGridStep));
            if (auto* mb = lay->getChildByName ("MAIN_BOUNDS"))
            {
                mainCustomBounds.clear();
                for (auto* e : mb->getChildIterator())
                    if (e->hasTagName ("B") && e->getStringAttribute ("k").isNotEmpty())
                        mainCustomBounds[e->getStringAttribute ("k")] = {
                            e->getIntAttribute ("x"), e->getIntAttribute ("y"),
                            std::max (16, e->getIntAttribute ("w", 40)), std::max (12, e->getIntAttribute ("h", 24))
                        };
            }
            if (laySchema >= 3)
            {
                if (auto* eb = lay->getChildByName ("EDIT_BOUNDS"))
                {
                    editCustomBounds.clear();
                    for (auto* e : eb->getChildIterator())
                        if (e->hasTagName ("B") && e->getStringAttribute ("k").isNotEmpty())
                            editCustomBounds[e->getStringAttribute ("k")] = {
                                e->getIntAttribute ("x"), e->getIntAttribute ("y"),
                                std::max (16, e->getIntAttribute ("w", 40)), std::max (12, e->getIntAttribute ("h", 24))
                            };
                }
                if (auto* eh = lay->getChildByName ("EDIT_HIDDEN"))
                {
                    editHiddenKeys.clear();
                    for (auto* e : eh->getChildIterator())
                        if (e->hasTagName ("H") && e->getStringAttribute ("k").isNotEmpty())
                            editHiddenKeys.insert (e->getStringAttribute ("k"));
                }
            }
            if (auto* mh = lay->getChildByName ("MAIN_HIDDEN"))
            {
                mainHiddenKeys.clear();
                for (auto* e : mh->getChildIterator())
                    if (e->hasTagName ("H") && e->getStringAttribute ("k").isNotEmpty())
                        mainHiddenKeys.insert (e->getStringAttribute ("k"));
            }
        }
    }

    {
        const juce::ScopedLock sl (zoneLock);
        for (auto& v : voices) v.active = false;
        legatoNoteStack.clear();
    }

    if (auto* groupsXml = xml.getChildByName ("GROUPS"))
    {
        const juce::ScopedLock sl (zoneLock);
        groups.clear();
        for (auto* gx : groupsXml->getChildIterator())
        {
            if (! gx->hasTagName ("GROUP")) continue;
            SampleGroup g;
            g.name = gx->getStringAttribute ("name", "Group");
            g.muted = gx->getIntAttribute ("muted", 0) != 0;
            g.soloed = gx->getIntAttribute ("soloed", 0) != 0;
            g.keyswitchNote = gx->getIntAttribute ("ksNote", -1);
            g.volumeDb100 = static_cast<float> (gx->getDoubleAttribute ("vol100", 80.0));
            g.pan50 = static_cast<float> (gx->getDoubleAttribute ("pan50", 0.0));

            for (auto* sx : gx->getChildIterator())
            {
                if (! sx->hasTagName ("FX")) continue;
                int rawType = sx->getIntAttribute ("type", 0);
                int targetSlot = -1;

                if (fmtVer >= 3)
                {
                    targetSlot = juce::jlimit (0, SampleGroup::numFxSlots - 1, sx->getIntAttribute ("idx", 0));
                }
                else
                {
                    if (rawType == 1) targetSlot = 0;
                    else if (rawType == 2) targetSlot = 1;
                    else if (rawType == 3) targetSlot = 2;
                    else if (rawType == 4) targetSlot = 5;
                    else if (rawType == 5) targetSlot = 6;
                }

                if (targetSlot >= 0 && targetSlot < SampleGroup::numFxSlots)
                {
                    auto& slot = g.fxSlots[static_cast<size_t> (targetSlot)];
                    slot.enabled = sx->getIntAttribute ("en", 0) != 0;
                    slot.p1 = static_cast<float> (sx->getDoubleAttribute ("p1", slot.p1));
                    slot.p2 = static_cast<float> (sx->getDoubleAttribute ("p2", slot.p2));
                    slot.p3 = static_cast<float> (sx->getDoubleAttribute ("p3", slot.p3));
                    slot.p4 = static_cast<float> (sx->getDoubleAttribute ("p4", slot.p4));
                    slot.p5 = static_cast<float> (sx->getDoubleAttribute ("p5", slot.p5));
                }
            }
            for (auto& slot : g.fxSlots)
                slot.prepare (currentSampleRate);
            groups.push_back (std::move (g));
        }
        if (groups.empty()) groups.emplace_back();
        selectedGroupIndex = 0;
    }

    if (auto* zonesXml = xml.getChildByName ("ZONES"))
    {
        {
            const juce::ScopedLock sl (zoneLock);
            zones.clear();
            selectedZoneIndex = -1;
        }
        for (auto* zx : zonesXml->getChildIterator())
        {
            if (! zx->hasTagName ("ZONE")) continue;
            int idx = createEmptyZone (zx->getIntAttribute ("rootKey", 60));
            int wantedLayerIdx = zx->getIntAttribute ("selLayer", 0);
            juce::String savedZoneName = zx->getStringAttribute ("name", "");
            bool savedRealRR = zx->getIntAttribute ("realRR", 1) != 0;
            {
                const juce::ScopedLock sl (zoneLock);
                auto& z = zones[static_cast<size_t> (idx)];
                z.name = savedZoneName.isNotEmpty() ? savedZoneName : z.name;
                z.groupIndex = juce::jlimit (0, std::max (0, static_cast<int> (groups.size()) - 1), zx->getIntAttribute ("groupIdx", 0));
                z.lowKey = juce::jlimit (0, 120, zx->getIntAttribute ("lowKey", 60));
                z.highKey = juce::jlimit (z.lowKey, 120, zx->getIntAttribute ("highKey", 60));
                z.rootKey = juce::jlimit (0, 120, zx->getIntAttribute ("rootKey", 60));
                z.lowVel = juce::jlimit (0, 127, zx->getIntAttribute ("lowVel", 0));
                z.highVel = juce::jlimit (z.lowVel, 127, zx->getIntAttribute ("highVel", 127));
                z.colourIndex = juce::jlimit (0, numPaletteColours - 1, zx->getIntAttribute ("colIdx", 0));
                z.colour = getPaletteColour (z.colourIndex);
                z.realRREnabled = savedRealRR;
                z.oneShot = zx->getIntAttribute ("oneShot", 0) != 0;
                z.chokeGroup = juce::jlimit (0, 8, zx->getIntAttribute ("chokeGrp", 0));

                z.startNorm = static_cast<float> (zx->getDoubleAttribute ("start", 0.0));
                z.endNorm = static_cast<float> (zx->getDoubleAttribute ("end", 1.0));
                z.loopStartNorm = static_cast<float> (zx->getDoubleAttribute ("lStart", 0.15));
                z.loopEndNorm = static_cast<float> (zx->getDoubleAttribute ("lEnd", 0.85));
                z.sampleLoop = zx->getIntAttribute ("sLoop", 0) != 0;
                z.autoLoopZeroCross = zx->getIntAttribute ("autoZLoop", 1) != 0;
                z.loopCrossfade = static_cast<float> (zx->getDoubleAttribute ("lXfade", 0.08));
                z.fadeInNorm = static_cast<float> (zx->getDoubleAttribute ("fIn", 0.0));
                z.fadeOutNorm = static_cast<float> (zx->getDoubleAttribute ("fOut", 0.0));
                z.reverse = zx->getIntAttribute ("rev", 0) != 0;
                z.pitchSemi = static_cast<float> (zx->getDoubleAttribute ("pitch", 0.0));
                z.pan50 = static_cast<float> (zx->getDoubleAttribute ("pan50", 0.0));
                z.volumeDb100 = static_cast<float> (zx->getDoubleAttribute ("vol100", 80.0));
                z.attackMs = static_cast<float> (zx->getDoubleAttribute ("atkMs", 0.10));
                z.decayMs = static_cast<float> (zx->getDoubleAttribute ("decMs", 300.0));
                z.sustainPct = static_cast<float> (zx->getDoubleAttribute ("susPct", 85.0));
                z.releaseMs = static_cast<float> (zx->getDoubleAttribute ("relMs", 250.0));
                z.cutoffHz = static_cast<float> (zx->getDoubleAttribute ("cut", 20000.0));
                z.resQ = static_cast<float> (zx->getDoubleAttribute ("res", 0.707));
            }
            if (auto* layersXml = zx->getChildByName ("LAYERS"))
            {
                for (auto* lx : layersXml->getChildIterator())
                {
                    juce::File f (lx->getStringAttribute ("file"));
                    if (f.existsAsFile()) addSampleToZone (idx, f);
                }
            }
            {
                const juce::ScopedLock sl (zoneLock);
                auto& z = zones[static_cast<size_t> (idx)];
                if (savedZoneName.isNotEmpty()) z.name = savedZoneName;
                z.realRREnabled = savedRealRR;
                if (! z.layers.empty())
                    z.selectedLayerIndex = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, wantedLayerIdx);
            }
            if (auto* seqXml = zx->getChildByName ("SEQ_EVENTS"))
            {
                const juce::ScopedLock sl (zoneLock);
                auto& z = zones[static_cast<size_t> (idx)];
                z.seqEvents.clear();
                for (auto* ex : seqXml->getChildIterator())
                {
                    RhythmEvent ev;
                    ev.position = static_cast<float> (ex->getDoubleAttribute ("pos", 0.0));
                    ev.velocity = static_cast<float> (ex->getDoubleAttribute ("vel", 0.85));
                    z.seqEvents.push_back (ev);
                }
            }
        }
        const juce::ScopedLock sl (zoneLock);
        selectedZoneIndex = zones.empty() ? -1 : 0;
    }

    recomputeSampleRamMb();
    stateRevision.fetch_add (1);
    return true;
}

bool RythmeAudioProcessor::savePresetMn6x (const juce::File& file)
{
    if (file == juce::File()) return false;
    file.getParentDirectory().createDirectory();
    auto xml = createFullStateXml (false);
    if (xml == nullptr) return false;
    if (! xml->writeTo (file)) return false;

    currentPresetFile = file;
    refreshLibraryFolderIfMatches (file);
    return true;
}

bool RythmeAudioProcessor::loadPresetMn6x (const juce::File& file)
{
    if (! file.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;

    currentPresetFile = file;
    bool ok = restoreFullStateFromXml (*xml, false);
    if (libraryNameText.trim().isEmpty())
        libraryNameText = file.getFileNameWithoutExtension();
    currentPresetName = libraryNameText;
    return ok;
}

void RythmeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createFullStateXml (true))
        copyXmlToBinary (*xml, destData);
}

void RythmeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xmlState = getXmlFromBinary (data, sizeInBytes);
    if (xmlState != nullptr)
        restoreFullStateFromXml (*xmlState, true);
}

juce::AudioProcessorEditor* RythmeAudioProcessor::createEditor()
{
    return new RythmeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RythmeAudioProcessor();
}
