%thread(1) {
#    pcap_in -d enp0s25 -d any -P -> $data_orig
    wsproto_in  -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -> $data_orig
    wsproto_in  -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -> $data_orig
    wsproto_in  -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -> $data_orig
    wsproto_in  -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -r /var/packets.mpproto -> $data_orig

    $data_orig | addlabelmember ITEM_NUMBER -c 0 | strlen CONTENT | calc '#TOTAL_LENGTH+=STRLEN;CONTENT_POSITION=#TOTAL_LENGTH;' -> $data_tagged
    $data_tagged| bundle -N 256 | workbalance -N 2 -J WCARDS -L CARD -> $data_in
    $data_orig | flush -N -t 4s -> $flush_var
    $data_orig | flush -N -t 2s -> $status_var
}
%thread(2){
    $data_in.CARD1 | workreceive -J WCARDS | unbundle -> $unb_1
    $flush_var, $unb_1 | timestamp -L TS_PRE -N -I -> $stamp0
    $status_var:STATUS, $stamp0| vectormatchnpu CONTENT -s CHAIN1 -R TOTAL_STATS -r INCR_STATS -L CHAIN1_MATCH -D "/sys/bus/pci/devices/0000:07:00.0" -C1 -F ../exprs.cut -M -m8 -v4 | timestamp -L TS_POST -N -I -> $data_mid1;
    $data_mid1.CHAIN1 | addlabelmember CHAIN -V CHAIN1 | bundle -N 1 -> $status_out;
    $data_mid1.CHAIN1_MATCH | bundle -N 256 -> $data_out;
}

%thread(3){
    $data_in.CARD0 | workreceive -J WCARDS | unbundle -> $unb_0
    $flush_var, $unb_0 | timestamp -L TS_PRE -N -I -> $stamp1
    $status_var:STATUS, $stamp1 |vectormatchnpu CONTENT -s CHAIN0 -R TOTAL_STATS -r INCR_STATS -L CHAIN0_MATCH  -D "/sys/bus/pci/devices/0000:07:00.0" -C0 -F ../exprs.cut -M  -m8 -v4  | timestamp -L TS_POST -N -I -> $data_mid0;
    $data_mid0.CHAIN0 | addlabelmember CHAIN -V CHAIN0 | bundle -N 1 -> $status_out;
    $data_mid0.CHAIN0_MATCH | bundle -N 256 -> $data_out;
}
%thread(4){
    $status_out | unbundle -> $unpacked_stat
    $data_out | unbundle -> $unbundled_out
    $unbundled_out | labelstat| print -VVT
#    $unbundled_out | label MATCH | mklabelset CONTENT -S -L TAGS | removefromtuple CONTENT -> $exit
#    $exit |  print -VVT
#    $exit | redisstream -M 65536 -P bench-stream
    $unpacked_stat | subtuple CHAIN INCR_STATS.DEVICE_TEMP INCR_STATS.BANDWIDTH INCR_STATS.EVENT_CNT INCR_STATS.EVENT_RATE TOTAL_STATS.BYTE_CNT INCR_STATS.HIT_CNT TOTAL_STATS.END_TS INCR_STATS.INTERVAL | redisstream -M 65536 -P status-stream
}
