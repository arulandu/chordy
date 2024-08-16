cp -r ./dist ./dmg
mv dmg dist/

create-dmg \
  --volname "Chordy" \
  --volicon "../py/src/data/favicon.icns" \
  --window-pos 200 120 \
  --window-size 600 300 \
  --icon-size 100 \
  --app-drop-link 425 120 \
  "dist/Chordy.dmg" \
  "dist/dmg"

rm -rf ./dist/dmg