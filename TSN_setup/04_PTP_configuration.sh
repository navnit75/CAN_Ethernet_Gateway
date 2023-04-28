gnome-terminal -t "PTP" --command="bash -c 'sudo ptp4l -i enp2s0 --step_threshold=1 -m; $SHELL'"
sudo pmc -u -b 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 clockAccuracy 0xfe offsetScaledLogVariance 0xffff currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 timeSource 0xa0"
gnome-terminal -t "PTP" --command="bash -c 'sudo phc2sys -s enp2s0 -c CLOCK_REALTIME --step_threshold=1 -w -m; $SHELL'"


