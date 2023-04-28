make
gnome-terminal -t "GATEWAY_NON_CRITICAL" --command="bash -c './gateway_non_critical; $SHELL'"
mv src/*.o build/obj/
cp gateway_non_critical build/bin/
