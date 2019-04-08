%thread(1) {
    wsproto_in  -r "${PACKETS}" -N "${TOTAL_MLEN}" -> $data_orig

    $data_orig | addlabelmember ITEM_NUMBER -c 0 | strlen CONTENT | calc '#TOTAL_LENGTH+=STRLEN;CONTENT_POSITION=#TOTAL_LENGTH;' -> $data_tagged
    $data_tagged| bundle -N 64 | workbalance -N 2 -J WCARDS -L CARD_ -> $data_in
    $data_orig | flush -N -t 4s -> $flush_var
}

%thread(2){
    $data_in.CARD_0 | workreceive -J WCARDS -> $inb_0
    $inb_0 | flush -N -C 16 -> $status_var_0
    $inb_0 | unbundle -> $unb_0
    $flush_var, $unb_0 | timestamp -L TS_PRE -N -I -> $stamp_0
    $status_var_0:STATUS, $stamp_0| vectormatchre2 CONTENT -s RE2_0 -R TOTAL_STATS -r INCR_STATS -L RE2_0_MATCH -F ../exprs.cut -M  ${MAX_MEM} | timestamp -L TS_POST -N -I -> $data_mid_0;
    $data_mid_0.RE2_0 | addlabelmember RE2 -V RE2_0 | bundle -N 1 -> $status_out;
    $data_mid_0.RE2_0_MATCH | bundle -N 128 -> $data_out;

}
%thread(3){
    $data_in.CARD_1 | workreceive -J WCARDS -> $inb_1
    $inb_1 | flush -N -C 16 -> $status_var_1
    $inb_1 | unbundle -> $unb_1

    $flush_var, $unb_1 | timestamp -L TS_PRE -N -I -> $stamp_1
    $status_var_1:STATUS, $stamp_1| vectormatchre2 CONTENT -s RE2_1 -R TOTAL_STATS -r INCR_STATS -L RE2_1_MATCH  -F ../exprs.cut -M ${MAX_MEM} | timestamp -L TS_POST -N -I -> $data_mid_1;
    $data_mid_1.RE2_1 | addlabelmember RE2 -V RE2_1 | bundle -N 1 -> $status_out;
    $data_mid_1.RE2_1_MATCH | bundle -N 128 -> $data_out;

}
%thread(4){
    $status_out | unbundle -> $unpacked_stat
    $data_out | unbundle -> $unbundled_out
    $unbundled_out | labelstat| print -VVT
    $unbundled_out | label MATCH | mklabelset CONTENT -S -L TAGS | removefromtuple CONTENT -> $exit

    $flush_var, $exit | redisstream -M 1048576 -B 256 -P "${PREFIX}:alerts"
	$flush_var, $unpacked_stat | redisstream -M 65536 -B 16 -E RE2 -H INCR_STATS -L INCREMENTAL -I BANDWIDTH -I EVENT_CNT -I EVENT_RATE -I HIT_CNT -I INTERVAL -I MAX_MATCHES -H TOTAL_STATS -L TOTAL -I EVENT_CNT -I HIT_CNT -I BANDWIDTH -I INTERVAL -I BYTE_CNT -I END_TS -I MAX_MATCHES -P "${PREFIX}:status"
}