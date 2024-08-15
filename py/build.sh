pyinstaller Chordy.spec

mkdir -p dist/dmg
rm -rf dist/dmg/*
cp -r dist/Chordy.app dist/dmg
test -f "dist/Chordy.dmg" && rm "dist/Chordy.dmg"

create-dmg \
  --volname "Chordy" \
  --volicon "./src/data/favicon.icns" \
  --window-pos 200 120 \
  --window-size 600 300 \
  --icon-size 100 \
  --icon "Chordy.app" 175 120 \
  --hide-extension "Chordy.app" \
  --app-drop-link 425 120 \
  "dist/Chordy.dmg" \
  "dist/dmg/"

cp dist/Chordy.dmg release/Chordy.dmg
cp dist/Chordy release/chordy
