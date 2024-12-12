#/bin/bash

buildPackage=false
uploadPackage=false
for arg in "$*"; do
  case "$arg" in
  --build-package)
    buildPackage=true ;;
  --upload-package)
    uploadPackage=true ;;
  esac
done

umask 0
cd /sources/deremotelauncher

apt update -y -q \
  && apt-get -y -q install software-properties-common \
  && add-apt-repository -y -u ppa:rpluess/dragondreams \
  && apt-get -y -q upgrade \
  && apt-get -y -q install libdelauncher-dev libdragengine-dev || exit 1

export SCONSFLAGS="-j 8"

# required since noble or git fails
git config --global --add safe.directory /sources/deremotelauncher

git clean -dfx || exit 1

fetchExternals() {
  scons lib_fox_fetch lib_denetwork_fetch || exit 1
}

writeIncludeBinaries() {
  FILE=debian/source/include-binaries
  echo `dir -1 extern/fox/fox-*.tar.bz2` >>$FILE
  echo `dir -1 extern/denetwork/denetworkcpp-unix-x64-*.tar.bz2` >>$FILE
}

cleanScons() {
  find -type d -name "__pycache__" | xargs -- rm -rf
  rm -f config.log
  rm -f build.log
  rm -rf .sconf_temp
  rm -f .sconsign.dblite
}

fetchExternals
writeIncludeBinaries
cleanScons

rm -rf /sources/deremotelauncher_*

if [ $buildPackage = true ]; then
  gbp buildpackage --git-debian-branch=debian --git-upstream-tree=debian \
     --git-ignore-new --git-force-create || exit 1
  ./debian/rules override_dh_auto_build || exit 1
  ./debian/rules override_dh_auto_install || exit 1
  ./debian/rules override_dh_shlibdeps || exit 1
else
  gbp export-orig --upstream-tree=debian --force-create || exit 1
fi

# gbp does not include the downloaded files in the source archive. fix it
FILE=`cd .. && dir -1 deremotelauncher_*.orig.tar.gz`
FILENOEXT=`echo $FILE | sed -e "s/.orig.tar.gz//" | sed -e "s/_/-/"`

gunzip ../deremotelauncher_*.orig.tar.gz || exit 1
tar --transform "s@^\(extern.*\)@$FILENOEXT/\\1@" -rf ../deremotelauncher_*.orig.tar \
  `dir -1 extern/fox/fox-*.tar.bz2` \
  `dir -1 extern/denetwork/denetworkcpp-unix-x64-*.tar.bz2` || exit 1
gzip ../deremotelauncher_*.orig.tar || exit 1

git clean -dfx || exit 1

# above clean also kills the prefetched externals and include binaries file.
# debuild now seems to need this suddenly
fetchExternals
writeIncludeBinaries
cleanScons

debuild -S -sa || exit 1

if [ $uploadPackage = true ]; then
  dput ppa:rpluess/dragondreams /sources/deremotelauncher_*_source.changes || exit 1
fi
