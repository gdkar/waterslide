%thread(1) {
    wsproto_in  -r /var/packets.mpproto -N "${TOTAL_MLEN}" -> $data_orig

    $data_orig | addlabelmember ITEM_NUMBER -c 0 | strlen CONTENT | calc '#TOTAL_LENGTH+=STRLEN;CONTENT_POSITION=#TOTAL_LENGTH;' -> $data_tagged
    $data_tagged| bundle -N 256 | workbalance -N 1 -J WCARDS -L CARD_ -> $data_in
    $data_orig | flush -N -t 4s -> $flush_var
}

%thread(2){
    $data_in.CARD_0 | workreceive -J WCARDS -> $inb_0
    $inb_0 | flush -N -C 16 -> $status_var_0
    $inb_0 | unbundle -> $unb_0
    $flush_var, $unb_0 | timestamp -L TS_PRE -N -I -> $stamp_0
    $status_var_0:STATUS, $stamp_0:TAG | vectormatchnpu CONTENT -s CHAIN_0 -R TOTAL_STATS -r INCR_STATS -L CHAIN_0_MATCH -D "${CARD}" -C0 -F ../exprs.cut -M -m "${NMATCHES}" -v6 | timestamp -L TS_MID -N -I -> $data_mid_0;

    $status_var_0:STATUS, $data_mid_0:TAG | vectormatchnpu CONTENT -s CHAIN_1 -R TOTAL_STATS -r INCR_STATS -L CHAIN_1_MATCH -D "${CARD}" -C1 -F ../exprs.cut -M -m "${NMATCHES}" -v6 | timestamp -L TS_POST -N -I -> $data_mid_1;
    $data_mid_1.CHAIN_0 | addlabelm CHAIN -V CHAIN_0 -> $chain_tagged
    $data_mid_1.CHAIN_1 | addlabelm CHAIN -V CHAIN_1 -> $chain_tagged
    $chain_tagged | bundle -N 16 -> $status_out;
    $data_mid_1.CHAIN_0_MATCH, $data_mid_1.CHAIN_1_MATCH | bundle -N 128 -> $data_out;

}
%thread(4){
    $status_out | unbundle -> $unpacked_stat
    $data_out | unbundle -> $unbundled_out
    $unbundled_out | labelstat| print -VVT
    $unbundled_out | label MATCH | mklabelset CONTENT -S -L TAGS | removefromtuple CONTENT -> $exit

    $flush_var, $exit | redisstream -M 1048576 -B 256 -P "${PREFIX}:alerts"
	$flush_var, $unpacked_stat | redisstream -M 65536 -B 16  -E CHAIN -H INCR_STATS -L INCREMENTAL -I MAX_MATCHES -I DEVICE_TEMP -I BANDWIDTH -I EVENT_CNT -I EVENT_RATE -I HIT_CNT -I INTERVAL -H TOTAL_STATS -L TOTAL -I EVENT_CNT -I HIT_CNT -I BANDWIDTH -I INTERVAL -I BYTE_CNT -I END_TS -P "${PREFIX}:status"
}
