cd ../pc1/gateway/non_critical/
sh clean.sh
make
sh build.sh
cd ../critical/tscf/
sh clean.sh
make
sh build.sh
cd ../ntscf/
sh clean.sh
make
sh build.sh
