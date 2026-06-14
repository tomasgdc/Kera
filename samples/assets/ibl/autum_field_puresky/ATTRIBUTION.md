# Autumn Field Pure Sky HDRI

Source https://polyhaven.com/a/autumn_field_puresky

License :@ CC0 1.0 Universal (CC0 1.0) Public Domain Dedication
License URL: https://creativecommons.org/publicdomain/zero/1.0/

Authors:
- Jarod Guest, sky edits
- Sergej Majborda, original

Generated files:
- generated/generated_ibl.ktx
- generated/generated_skybox.ktx
- generated/sh.txt

Generation command:
```
.\cmgen.exe --deploy=samples\assets\ibl\autum_field_puresky\generated --format=ktx --size=256 samples\assets\ibl\autum_field_puresky\autumn_field_puresky_4k.exr
```

Original source:
- autum_field_puresky_4k.exr

Notes:
- Used as the outdoor image-based lighting, environment for the Damanged Helmet IBL sample
- The `.ktx` files are generated from the original `.exr` file using the `filament\cmgen
- Attribution is not required for CC0 assets, but this file is kept for provenance.