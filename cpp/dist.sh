if [ ! -d ./dist ]; then
    mkdir ./dist && mkdir ./dist/res
fi

cp ./build/chordy ./dist && cp -R ./build/res ./dist/res
zip dist.zip dist;