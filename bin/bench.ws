%thread(1) {
#    pcap_in -d enp0s25 -d any -P -> $data_orig
    wsproto_in  -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -> $data_orig
    wsproto_in  -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -> $data_orig
    wsproto_in  -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -> $data_orig

    $data_orig | addlabelmember ITEM_NUMBER -c 0 | strlen CONTENT | calc '#TOTAL_LENGTH+=STRLEN;CONTENT_POSITION=#TOTAL_LENGTH;' -> $data_tagged
    $data_tagged| bundle -N 256 | workbalance -N 1 -J WCARDS -L CARD_ -> $data_in
    $data_orig | flush -N -t 4s -> $flush_var
    $data_orig | flush -N -t 2s -> $status_var
}

%thread(04){
    $data_in.CARD_0 | workreceive -J WCARDS | unbundle -> $unb_0
    $flush_var, $unb_0 | timestamp -L TS_PRE -N -I -> $stamp_0
    $status_var:STATUS, $stamp_0| vectormatchnpu CONTENT -s CHAIN_0 -R TOTAL_STATS -r INCR_STATS -L CHAIN_0_MATCH -D "${CARD}" -F ../exprs.cut -M -m2 -v4 | timestamp -L TS_POST -N -I -> $data_mid_0;
    $data_mid_0.CHAIN_0 | addlabelmember CHAIN -V CHAIN_0 | bundle -N 1 -> $status_out;
    $data_mid_0.CHAIN_0_MATCH | bundle -N 256 -> $data_out;

}
#%thread(05){
#    $data_in.CARD_1 | workreceive -J WCARDS | unbundle -> $unb_1
#    $flush_var, $unb_1 | timestamp -L TS_PRE -N -I -> $stamp_1
#    $status_var:STATUS, $stamp_1| vectormatchnpu CONTENT -s CHAIN_1 -R TOTAL_STATS -r INCR_STATS -L CHAIN_1_MATCH -D "${CARD}" -C1 -F ../exprs.cut -M -m2 -v4 | timestamp -L TS_POST -N -I -> $data_mid_1;
#    $data_mid_1.CHAIN_1 | addlabelmember CHAIN -V CHAIN_1 | bundle -N 1 -> $status_out;
#    $data_mid_1.CHAIN_1_MATCH | bundle -N 256 -> $data_out;
#}
#%thread(204){
#    $data_in.CARD_1 | workreceive -J WCARDS | unbundle -> $unb_1
#    $flush_var, $unb_1 | timestamp -L TS_PRE -N -I -> $stamp_1
#    $status_var:STATUS, $stamp_1| vectormatchnpu CONTENT -s CHAIN_1 -R TOTAL_STATS -r INCR_STATS -L CHAIN_1_MATCH -D "/sys/bus/pci/devices/0000:01:00.0" -C0 -F ../exprs.cut -M -m8 -v4 | timestamp -L TS_POST -N -I -> $data_mid_1;
#    $data_mid_1.CHAIN_1 | addlabelmember CHAIN -V CHAIN_1 | bundle -N 1 -> $status_out;
#    $data_mid_1.CHAIN_1_MATCH | bundle -N 256 -> $data_out;
#}
#%thread(205){
#    $data_in.CARD_0 | workreceive -J WCARDS | unbundle -> $unb_0
#    $flush_var, $unb_0 | timestamp -L TS_PRE -N -I -> $stamp_0
#    $status_var:STATUS, $stamp_0| vectormatchnpu CONTENT -s CHAIN_0 -R TOTAL_STATS -r INCR_STATS -L CHAIN_0_MATCH -D "/sys/bus/pci/devices/0000:01:00.0" -C1 -F ../exprs.cut -M -m8 -v4 | timestamp -L TS_POST -N -I -> $data_mid_0;
#    $data_mid_0.CHAIN_0 | addlabelmember CHAIN -V CHAIN_0 | bundle -N 1 -> $status_out;
#    $data_mid_0.CHAIN_0_MATCH | bundle -N 256 -> $data_out;
#}
#%thread(304){
#    $data_in.CARD_4 | workreceive -J WCARDS | unbundle -> $unb_4
#    $flush_var, $unb_4 | timestamp -L TS_PRE -N -I -> $stamp_4
#    $status_var:STATUS, $stamp_4| vectormatchnpu CONTENT -s CHAIN_4 -R TOTAL_STATS -r INCR_STATS -L CHAIN_4_MATCH -D "/sys/bus/pci/devices/0000:03:00.0" -C0 -F ../exprs.cut -M -m8 -v4 | timestamp -L TS_POST -N -I -> $data_mid_4;
#    $data_mid_4.CHAIN_4 | addlabelmember CHAIN -V CHAIN_4 | bundle -N 1 -> $status_out;
#    $data_mid_4.CHAIN_4_MATCH | bundle -N 256 -> $data_out;
#}
#%thread(305){
#    $data_in.CARD_5 | workreceive -J WCARDS | unbundle -> $unb_5
#    $flush_var, $unb_5 | timestamp -L TS_PRE -N -I -> $stamp_5
#    $status_var:STATUS, $stamp_5| vectormatchnpu CONTENT -s CHAIN_5 -R TOTAL_STATS -r INCR_STATS -L CHAIN_5_MATCH -D "/sys/bus/pci/devices/0000:03:00.0" -C1 -F ../exprs.cut -M -m8 -v4 | timestamp -L TS_POST -N -I -> $data_mid_5;
#    $data_mid_5.CHAIN_5 | addlabelmember CHAIN -V CHAIN_5 | bundle -N 1 -> $status_out;
#    $data_mid_5.CHAIN_5_MATCH | bundle -N 256 -> $data_out;
#}
%thread(4){
    $status_out | unbundle -> $unpacked_stat
    $data_out | unbundle -> $unbundled_out
    $unbundled_out | labelstat| print -VVT
#    $unbundled_out | label MATCH | mklabelset CONTENT -S -L TAGS | removefromtuple CONTENT -> $exit
#    $exit |  print -VVT
#    $exit | redisstream -M 65536 -P bench-stream
    $unpacked_stat | subtuple CHAIN INCR_STATS.DEVICE_TEMP INCR_STATS.BANDWIDTH INCR_STATS.EVENT_CNT INCR_STATS.EVENT_RATE TOTAL_STATS.BYTE_CNT INCR_STATS.HIT_CNT TOTAL_STATS.END_TS INCR_STATS.INTERVAL | redisstream -M 65536 -P status-stream
}
