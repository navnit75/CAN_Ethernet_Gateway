make
gnome-terminal -t "GATEWAY_CRITICAL" --command="bash -c 'sudo ./gateway_critical; $SHELL'"
mv src/*.o build/obj/
cp gateway_critical build/bin/
mv avtp/src/*.o avtp/build/obj/
