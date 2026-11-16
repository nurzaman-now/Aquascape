$fn = 60;

posisi_poros_x = 9.5; posisi_poros_y = 12.0;
posisi_lubang_x = 39.0; posisi_lubang_y = 11.0;
jarak_poros_ke_lubang = sqrt(pow(posisi_lubang_x - posisi_poros_x, 2) + pow(posisi_lubang_y - posisi_poros_y, 2));
diameter_pelat_penutup = 16.0;

horn_tebal = 2.0; // 2.0 mm kedalaman ceruk horn bawaan MG90S
horn_panjang_lengan = 15.5;
horn_lebar_pangkal = 7.6;
horn_lebar_ujung = 4.4;
tebal_katup_total = horn_tebal + 1.4;

module bentuk_katup_dasar() {
    difference() {
        union() {
            cylinder(h = tebal_katup_total, d = 12.0);
            hull() {
                cylinder(h = tebal_katup_total, d = 10.6);
                translate([jarak_poros_ke_lubang, 0, 0]) cylinder(h = tebal_katup_total, d = 8.5);
            }
            translate([jarak_poros_ke_lubang, 0, 0]) cylinder(h = tebal_katup_total, d = diameter_pelat_penutup);
        }
        translate([0, 0, -0.1])
            hull() {
                cylinder(h = horn_tebal + 0.1, d = horn_lebar_pangkal);
                translate([horn_panjang_lengan, 0, 0]) cylinder(h = horn_tebal + 0.1, d = horn_lebar_ujung);
            }
        translate([0, 0, -1]) cylinder(h = tebal_katup_total + 2, d = 7.6);
        translate([9.5, 0, -1]) cylinder(h = tebal_katup_total + 2, d = 1.5);
        translate([14.0, 0, -1]) cylinder(h = tebal_katup_total + 2, d = 1.5);
    }
}

module katup_horn(siap_print = true) {
    if (siap_print) {
        // Dibalik agar ceruk horn menghadap ke atas saat dicetak tanpa support
        translate([0, 0, tebal_katup_total])
        rotate([180, 0, 0])
            bentuk_katup_dasar();
    } else {
        // Posisi normal tegak untuk dipasang ke poros servo
        bentuk_katup_dasar();
    }
}

katup_horn(siap_print = true);
