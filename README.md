# Noise Mu6ix Rhythm v1.0.4 (VST3)

## v1.0.4-ல் செய்துள்ள முக்கிய மாற்றங்கள்:
1. **VST3 ஃபைல் பெயரில் வெர்ஷன்:** அவுட்புட் ஃபைலே `Noise Mu6ix Rhythm v1.0.4.vst3` என உருவாகும் (`build.yml`-ல் `*.vst3` பயன்படுத்தப்பட்டுள்ளதால் இனி எளிதாக பில்ட் ஆகும்).
2. **ஒவ்வொரு இன்ஸ்ட்ருமென்ட்டுக்கும் தனித்தனி Wave Editor செட்டிங்ஸ்:** `Start`, `End`, `L-Start`, `L-End`, `LOOP`, `Pitch`, `Pan`, `Vol`, `ADSR`, `Cutoff`, `Res` ஆகிய அனைத்தும் ஒவ்வொரு கீ ஜோனுக்கும் தனித்தனியாகச் செயல்படும் & `.mn6x`-ல் சேவ் ஆகும்.
3. **Top Navigation Tabs (ஸ்க்ரோல் இல்லாமல்):** மேலே **`MAIN | KEY MAP | WAVE | RHYTHM | EFFECTS | MIXER`** பட்டன்கள் சேர்க்கப்பட்டுள்ளன.
4. **10 Octaves (C0–C10):** Key Mapper மற்றும் கீழே உள்ள பியானோ கீபோர்டு இரண்டும் முழு 10 ஆக்டேவ்களுக்கு விரிவாக்கப்பட்டுள்ளன.
5. **Global Effects Tab (`EFFECTS`):** 8 Insert Slots — EQ, Compressor, Saturation, Filter, Stereo Delay, Space Reverb, Stereo Width ஆகியவற்றை இன்செர்ட் செய்து ON/OFF மற்றும் நாப்களால் கண்ட்ரோல் செய்யலாம்.
6. **Dedicated Mixer Tab (`MIXER`):** ஒவ்வொரு கீ ஜோனுக்கும் அதன் நிறத்திலேயே தனி Mixer Strip — `Out 1` முதல் `Out 8` வரை Multi-Out Routing, Volume, Pan, Mute/Solo மற்றும் **`+ FX`** வழியாக அன்லிமிடெட் Per-Channel Insert Effects சேர்க்கலாம்.
7. **CPU-Friendly Engine:** சத்தமில்லாத/பயன்படுத்தாத ஜோன்கள் மற்றும் OFF செய்யப்பட்ட எஃபெக்ட்கள் 0% CPU-வே எடுக்கும் வகையில் இலகுவான DSP-ஆக உருவாக்கப்பட்டுள்ளது.
