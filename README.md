# RYTHME v1.2.3 (VST3)

## இந்த v1.2.3 பதிப்பில் சேர்க்கப்பட்டவை:
1. **Sequential Auto-Versioning & Inner `.vst3` Version Name:**
   - `CMakeLists.txt`-ல் `PRODUCT_NAME "RYTHME ${RYTHME_VERSION_STR}"` என அமைக்கப்பட்டதால், டவுன்லோட் செய்யும் `.vst3` ஃபோல்டர் மற்றும் உள்ளே இருக்கும் `.vst3` பைனரி இரண்டும் **`RYTHME v1.2.3.vst3`** என வெர்ஷனுடனேயே உருவாகும்; அடுத்தடுத்த பில்ட்களில் தானாக `v1.2.4`, `v1.2.5` என மாறும்.
2. **Vintage Black & White Scrolling Output Waveform Display (`MAIN`):**
   - கீ-யை அழுத்தி வாசிக்கும்போது அவுட்புட் அலைவடிவம் வலமிருந்து இடமாக ஸ்க்ரோல் ஆகி ஓடும்; கையை எடுத்ததும் சுத்தமாகக் காலியாக (`Empty`) இருக்கும்.
3. **Top-Right Settings (`⚙`) + Logo/Title → `Settings`:**
   - `EDIT PANEL` டேப் நீக்கப்பட்டு, `MAIN` மற்றும் `EDIT` பக்கங்களின் மேல் வலது மூலையில் `⚙` ஐகான் வைக்கப்பட்டுள்ளது (`Grid: Free / 10x..50x`, Drag & Bottom-Right Resize, `DONE` & `Reset`).
   - Logo/Title மெனுவில் `Settings` கிளிக் செய்தால் Logo Shape, Logo Colour, Logo Letters, Plugin Title எல்லாம் ஒரே இடத்தில் அருகருகே திறக்கும் (`Apply / OK`).
4. **Type-In + Vertical Drag + Double-Click Default & Center-Zero Pan:**
   - `Pitch` (`-24..+24`), `Volume` (`0..100 dB`, Default `80`), `Attack` (`0.10 ms`), `Decay` (`300 ms`), `Sustain` (`85%`), `Release` (`250 ms`) + கீழே **ADSR Graph** (Curve நீக்கம்).
   - `Pan` & `Group Pan` Center `0` புள்ளியுடன் (`-50` இடது நிறம், `+50` வலது நிறம்).
5. **Clean FRR + Cut-Only FVL + FabFilter-Style Effects (`Pro-EQ`, `Pro-Comp`, `Saturn-Sat`, `Timeless-Dly`, `Pro-Rev`):**
   - `FVL`-ல் உங்கள் ஒரிஜினல் சாம்பிள் 100% Max; குறைந்த வெலாசிட்டிகளில் ஹை ஃப்ரீக்வென்சி மென்மையாக கட் ஆகும்.
