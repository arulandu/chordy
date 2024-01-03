pyinstaller chordy.spec
#   --icon "chordy.app" 175 120 \

mkdir -p dist/dmg
rm -rf dist/dmg/*
cp -r dist/chordy.app dist/dmg
test -f "dist/chordy.dmg" && rm "dist/chordy.dmg"

create-dmg \
  --volname "Chordy" \
  --volicon "./src/icon.icns" \
  --window-pos 200 120 \
  --window-size 600 300 \
  --icon-size 100 \
  --icon "chordy.app" 175 120 \
  --hide-extension "chordy.app" \
  --app-drop-link 425 120 \
  "dist/Chordy.dmg" \
  "dist/dmg/"