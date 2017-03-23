%thread(1) {
wsproto_in -r '../../waterslide.git/npu2_bench/packets.*.wsproto' | addlabelmember -c COUNT -> $wdata_in
$wdata_in | bundle -N 256 | workbalance -N 32 -J CORES -L CORE -> $core
}
%thread(0) {
$re2_out | labelstat | print -V
}
%thread(2) {
$core.CORE0 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(3) {
$core.CORE1 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(4) {
$core.CORE2 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(5) {
$core.CORE3 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(6) {
$core.CORE4 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(7) {
$core.CORE5 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(8) {
$core.CORE6 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(9) {
$core.CORE7 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(10) {
$core.CORE8 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(11) {
$core.CORE9 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(12) {
$core.CORE10 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(13) {
$core.CORE11 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(14) {
$core.CORE12 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(15) {
$core.CORE13 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(16) {
$core.CORE14 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(17) {
$core.CORE15 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}

%thread(18) {
$core.CORE16 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(19) {
$core.CORE17 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(20) {
$core.CORE18 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(21) {
$core.CORE19 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(22) {
$core.CORE20 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(23) {
$core.CORE21 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(24) {
$core.CORE22 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(25) {
$core.CORE23 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(26) {
$core.CORE24 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(27) {
$core.CORE25 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(28) {
$core.CORE26 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(29) {
$core.CORE27 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(30) {
$core.CORE28 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(31) {
$core.CORE29 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(32) {
$core.CORE30 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
%thread(33) {
$core.CORE31 | workreceive -J CORES | unbundle | vectormatchre2 CONTENT -F ../../waterslide.git/bench_regexes.list -M -L RE2_MATCH -> $re2_out
}
