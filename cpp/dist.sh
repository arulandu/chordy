test -d ./dist && rm -rf ./dist
mkdir -p ./dist/res

cp ./build/chordy ./dist && cp -R ./build/res ./dist/
