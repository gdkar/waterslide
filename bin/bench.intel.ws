%thread(22) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig0
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig0
    $data_orig0 | bundle -N 512 -> $data_in0
    $data_orig0 | flush -N -t 8s -> $flush_var0
    $data_orig0 | flush -N -t 4s -> $status_var0
}
%thread(24) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig1
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig1
    $data_orig1 | bundle -N 512 -> $data_in1
    $data_orig1 | flush -N -t 8s -> $flush_var1
    $data_orig1 | flush -N -t 4s -> $status_var1
}
%thread(23) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig2
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig2
    $data_orig2 | bundle -N 512 -> $data_in2
    $data_orig2 | flush -N -t 8s -> $flush_var2
    $data_orig2 | flush -N -t 4s -> $status_var2
}
%thread(25) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig3
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig3
    $data_orig3 | bundle -N 512 -> $data_in3
    $data_orig3 | flush -N -t 8s -> $flush_var3
    $data_orig3 | flush -N -t 4s -> $status_var3
}
%thread(26) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig4
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig4
    $data_orig4 | bundle -N 512 -> $data_in4
    $data_orig4 | flush -N -t 8s -> $flush_var4
    $data_orig4 | flush -N -t 4s -> $status_var4
}
%thread(28) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig5
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig5
    $data_orig5 | bundle -N 512 -> $data_in5
    $data_orig5 | flush -N -t 8s -> $flush_var5
    $data_orig5 | flush -N -t 4s -> $status_var5
}
%thread(27) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig6
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig6
    $data_orig6 | bundle -N 512 -> $data_in6
    $data_orig6 | flush -N -t 8s -> $flush_var6
    $data_orig6 | flush -N -t 4s -> $status_var6
}
%thread(29) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig7
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig7
    $data_orig7 | bundle -N 512 -> $data_in7
    $data_orig7 | flush -N -t 8s -> $flush_var7
    $data_orig7 | flush -N -t 4s -> $status_var7
}
%thread(30) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig8
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig8
    $data_orig8 | bundle -N 512 -> $data_in8
    $data_orig8 | flush -N -t 8s -> $flush_var8
    $data_orig8 | flush -N -t 4s -> $status_var8
}
%thread(32) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig9
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig9
    $data_orig9 | bundle -N 512 -> $data_in9
    $data_orig9 | flush -N -t 8s -> $flush_var9
    $data_orig9 | flush -N -t 4s -> $status_var9
}
%thread(31) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig10
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig10
    $data_orig10 | bundle -N 512 -> $data_in10
    $data_orig10 | flush -N -t 8s -> $flush_var10
    $data_orig10 | flush -N -t 4s -> $status_var10
}
%thread(33) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig11
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig11
    $data_orig11 | bundle -N 512 -> $data_in11
    $data_orig11 | flush -N -t 8s -> $flush_var11
    $data_orig11 | flush -N -t 4s -> $status_var11
}
%thread(34) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig12
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig12
    $data_orig12 | bundle -N 512 -> $data_in12
    $data_orig12 | flush -N -t 8s -> $flush_var12
    $data_orig12 | flush -N -t 4s -> $status_var12
}
%thread(36) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig13
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig13
    $data_orig13 | bundle -N 512 -> $data_in13
    $data_orig13 | flush -N -t 8s -> $flush_var13
    $data_orig13 | flush -N -t 4s -> $status_var13
}
%thread(35) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig14
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig14
    $data_orig14 | bundle -N 512 -> $data_in14
    $data_orig14 | flush -N -t 8s -> $flush_var14
    $data_orig14 | flush -N -t 4s -> $status_var14
}
%thread(37) {
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig15
    wsproto_in -r /tmp/packets.mpproto -r /tmp/packets.mpproto -> $data_orig15
    $data_orig15 | bundle -N 512 -> $data_in15
    $data_orig15 | flush -N -t 8s -> $flush_var15
    $data_orig15 | flush -N -t 4s -> $status_var15
}
%thread(2){
    $data_in0 | unbundle -> $unb_CARD0
    $status_var0:STATUS, $flush_var0, $unb_CARD0 | vectormatchnpu CONTENT -s STATS_CARD0  -r INCR_STATS -L CARD0_MATCH  -D "/sys/bus/pci/devices/0000:06:00.0" -C0 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_mid0;
    $data_mid0.STATS_CARD0 | print -V
#    $data_mid0.CARD0_MATCH | bundle -N 512 -> $data_out;
}
%thread(4){
    $data_in1 | unbundle -> $unb_CARD1
    $status_var1:STATUS, $flush_var10, $unb_CARD1 | vectormatchnpu CONTENT -s STATS_CARD1  -r INCR_STATS -L CARD1_MATCH -D "/sys/bus/pci/devices/0000:06:00.0" -C1 -M -F ../exprs.cut  -m "${FC_MATCH_COUNT}" -v6 -> $data_mid1;
#    $data_mid1.STATS_CARD1 |print -V
    $data_mid1.CARD1_MATCH | bundle -N 512 -> $data_out;
}
%thread(3){
    $data_in2 | unbundle -> $unb_CARD2
    $status_var2:STATUS, $flush_var2, $unb_CARD2 | vectormatchnpu CONTENT -s STATS_CARD2  -r INCR_STATS -L CARD2_MATCH  -D "/sys/bus/pci/devices/0000:07:00.0" -C0 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD2;
    $data_CARD2.STATS_CARD2 | print -V
#    $data_CARD2.CARD2_MATCH | bundle -N 512 -> $data_out;
}
%thread(5){
    $data_in3 | unbundle -> $unb_CARD3
    $status_var3:STATUS, $flush_var3, $unb_CARD3 | vectormatchnpu CONTENT -s STATS_CARD3  -r INCR_STATS -L CARD3_MATCH  -D "/sys/bus/pci/devices/0000:07:00.0" -C1 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD3;
    $data_CARD3.STATS_CARD3 | print -V
#    $data_CARD3.CARD3_MATCH | bundle -N 512 -> $data_out;
}
%thread(6){
    $data_in4 | unbundle -> $unb_CARD4
    $status_var4:STATUS, $flush_var4, $unb_CARD4 | vectormatchnpu CONTENT -s STATS_CARD4  -r INCR_STATS -L CARD4_MATCH  -D "/sys/bus/pci/devices/0000:0a:00.0" -C0 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD4;
    $data_CARD4.STATS_CARD4 | print -V
#    $data_CARD4.CARD4_MATCH | bundle -N 512 -> $data_out;
}
%thread(8){
    $data_in5 | unbundle -> $unb_CARD5
    $status_var5:STATUS, $flush_var5, $unb_CARD5 | vectormatchnpu CONTENT -s STATS_CARD5  -r INCR_STATS -L CARD5_MATCH  -D "/sys/bus/pci/devices/0000:0a:00.0" -C1 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD5;
    $data_CARD5.STATS_CARD5 | print -V
#    $data_CARD5.CARD5_MATCH | bundle -N 512 -> $data_out;
}
%thread(7){
    $data_in6 | unbundle -> $unb_CARD6
    $status_var6:STATUS, $flush_var6, $unb_CARD6 | vectormatchnpu CONTENT -s STATS_CARD6  -r INCR_STATS -L CARD6_MATCH  -D "/sys/bus/pci/devices/0000:0b:00.0" -C0 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD6;
    $data_CARD6.STATS_CARD6 |  print -V
#    $data_CARD6.CARD6_MATCH | bundle -N 512 -> $data_out;
}
%thread(9){
    $data_in7 | unbundle -> $unb_CARD7
    $status_var7:STATUS, $flush_var7, $unb_CARD7 | vectormatchnpu CONTENT -s STATS_CARD7  -r INCR_STATS -L CARD7_MATCH  -D "/sys/bus/pci/devices/0000:0b:00.0" -C1 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD7;
    $data_CARD7.STATS_CARD7 |  print -V
#    $data_CARD7.CARD7_MATCH | bundle -N 512 -> $data_out;
}
%thread(10){
    $data_in8 | unbundle -> $unb_CARD8
    $status_var8:STATUS, $flush_var8, $unb_CARD8 | vectormatchnpu CONTENT -s STATS_CARD8  -r INCR_STATS -L CARD8_MATCH  -D "/sys/bus/pci/devices/0000:84:00.0" -C0 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD8;
    $data_CARD8.STATS_CARD8 |  print -V
#    $data_CARD8.CARD8_MATCH | bundle -N 512 -> $data_out;
}
%thread(12){
    $data_in9 | unbundle -> $unb_CARD9
    $status_var9:STATUS, $flush_var9, $unb_CARD9 | vectormatchnpu CONTENT -s STATS_CARD9  -r INCR_STATS -L CARD9_MATCH  -D "/sys/bus/pci/devices/0000:84:00.0" -C1 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD9;
    $data_CARD9.STATS_CARD9 |  print -V
#    $data_CARD9.CARD9_MATCH | bundle -N 512 -> $data_out;
}
%thread(11){
    $data_in10 | unbundle -> $unb_CARD10
    $status_var10:STATUS, $flush_var10, $unb_CARD10 | vectormatchnpu CONTENT -s STATS_CARD10  -r INCR_STATS -L CARD10_MATCH  -D "/sys/bus/pci/devices/0000:85:00.0" -C0 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD10;
    $data_CARD10.STATS_CARD10 |  print -V
#    $data_CARD10.CARD10_MATCH | bundle -N 512 -> $data_out;
}
%thread(13){
    $data_in11 | unbundle -> $unb_CARD11
    $status_var11:STATUS, $flush_var11, $unb_CARD11 | vectormatchnpu CONTENT -s STATS_CARD11  -r INCR_STATS -L CARD11_MATCH  -D "/sys/bus/pci/devices/0000:85:00.0" -C1 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD11;
    $data_CARD11.STATS_CARD11 |  print -V
#    $data_CARD11.CARD11_MATCH | bundle -N 512 -> $data_out;
}
%thread(14){
    $data_in12 | unbundle -> $unb_CARD12
    $status_var12:STATUS, $flush_var12, $unb_CARD12 | vectormatchnpu CONTENT -s STATS_CARD12  -r INCR_STATS -L CARD12_MATCH  -D "/sys/bus/pci/devices/0000:88:00.0" -C0 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD12;
    $data_CARD12.STATS_CARD12 |  print -V
#    $data_CARD12.CARD12_MATCH | bundle -N 512 -> $data_out;
}
%thread(16){
    $data_in13 | unbundle -> $unb_CARD13
    $status_var13:STATUS, $flush_var13, $unb_CARD13 | vectormatchnpu CONTENT -s STATS_CARD13  -r INCR_STATS -L CARD13_MATCH  -D "/sys/bus/pci/devices/0000:88:00.0" -C1 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD13;
    $data_CARD13.STATS_CARD13 |  print -V
#    $data_CARD13.CARD13_MATCH | bundle -N 512 -> $data_out;
}
%thread(15){
    $data_in14 | unbundle -> $unb_CARD14
    $status_var14:STATUS, $flush_var14, $unb_CARD14 | vectormatchnpu CONTENT -s STATS_CARD14  -r INCR_STATS -L CARD14_MATCH  -D "/sys/bus/pci/devices/0000:89:00.0" -C0 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD14;
    $data_CARD14.STATS_CARD14 |  print -V
#    $data_CARD14.CARD14_MATCH | bundle -N 512 -> $data_out;
}
%thread(17){
    $data_in15 | unbundle -> $unb_CARD15
    $status_var15:STATUS, $flush_var15, $unb_CARD15 | vectormatchnpu CONTENT -s STATS_CARD15  -r INCR_STATS -L CARD15_MATCH  -D "/sys/bus/pci/devices/0000:89:00.0" -C1 -M -F ../exprs.cut   -m "${FC_MATCH_COUNT}" -v6 -> $data_CARD15;
    $data_CARD15.STATS_CARD15 |  print -V
#    $data_CARD15.CARD15_MATCH | bundle -N 512 -> $data_out;
}
#%thread(20){
#$status_out | unbundle | print -V;
#$data_out | unbundle | labelstat| print -VVT
#}
