# GLES driver modules

This directory is where the PowerVR SGX driver modules go before the VPK is
packed. They are **not** redistributed in this repository; download them from
the [PVR_PSP2](https://github.com/GrapheneCt/PVR_PSP2) release that matches the
headers and stubs the build was compiled against:

```bash
PVR_VERSION=3.9
curl -L -o PSVita_Release.zip \
  "https://github.com/GrapheneCt/PVR_PSP2/releases/download/v${PVR_VERSION}/PSVita_Release.zip"
unzip -j PSVita_Release.zip -d vita/module \
  '*/libgpu_es4_ext.suprx' '*/libIMGEGL.suprx' \
  '*/libGLESv2.suprx' '*/libpvrPSP2_WSEGL.suprx'
```

The four files SDL loads at start-up are:

- `libgpu_es4_ext.suprx`
- `libIMGEGL.suprx`
- `libGLESv2.suprx`
- `libpvrPSP2_WSEGL.suprx`

CMake warns for each one that is missing and still produces a VPK, but that VPK
will fail to create a GL context and boot to a black screen.
