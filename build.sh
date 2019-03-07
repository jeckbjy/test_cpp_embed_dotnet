#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}")" && pwd )
ROOT_DIR=${DIR}
SRC_DIR=$ROOT_DIR/src
OUT_DIR=$ROOT_DIR/bin

mkdir -p $OUT_DIR

# if [ ! -e "$OUT_DIR/libcoreclr.dylib" ]; then
#     cp "/usr/local/share/dotnet/shared/Microsoft.NETCore.App/2.0.0/libcoreclr.dylib" $OUT_DIR/
# fi

# build csharp project
# dotnet publish --self-contained -r osx-x64 ${SRC_DIR}/ManagedLibrary/ManagedLibrary.csproj -o ${OUT_DIR}
# dotnet publish -r osx-x64 ${SRC_DIR}/ManagedLibrary/ManagedLibrary.csproj -o ${OUT_DIR}
dotnet build -r osx-x64 ${SRC_DIR}/ManagedLibrary/ManagedLibrary.csproj -o ${OUT_DIR}

# build cpp host exe
g++ -o ${OUT_DIR}/host ${SRC_DIR}/host.cpp -ldl
