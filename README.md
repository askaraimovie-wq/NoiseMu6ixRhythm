# RYTHME (Auto-Versioning VST3)

## இந்த வெளியீட்டில் சேர்க்கப்பட்டுள்ளவை:
1. **Sequential Auto-Versioning & `.vst3` பெயர்:**
   - `v1.2.2` → `v1.2.3` → `v1.2.4` → `v1.2.5` → `v1.2.6` எனத் தானாகத் தொடரும் வகையில் `.github/workflows/build.yml` மற்றும் `CMakeLists.txt` அமைக்கப்பட்டுள்ளன.
   - அவுட்புட் ஃபைல் மற்றும் பிளக்கின் பெயரில் வெர்ஷன் சேர்ந்து வரும் (`RYTHME v1.2.2.vst3` போன்றது).
2. **`MAIN`, `EDIT`, மற்றும் `EDIT PANEL`:**
   - `EDIT PANEL`-க்குள் `Grid: Free`, `10x`, `20x`, `30x`, `40x`, `50x`, `Knob Style 1..10`, `Fader Style 1..10`, `Switch Style 1..10`, Logo/Title எடிட்டர், Patcher-style Node/Wire இணைப்பு (`+ Add Node`, இழுத்து நகர்த்தல்/கீழ்-வலது Resize, Port இணைப்பு), கீழே `MAIN PAGE` / `EDIT PAGE` தேர்வு மற்றும் `DONE` பட்டன்.
3. **`MAIN` பக்கத்தில் Live Waveform Display:**
   - S/E மார்க்கர்கள் இல்லாமல் பிளே ஆகும் கீ-யின் Waveform மற்றும் Playhead காட்டும் டிஸ்ப்ளே.
