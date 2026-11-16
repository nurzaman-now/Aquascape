$fn = 60;

panjang_x = 50.0; lebar_y = 50.0; tinggi_bawah_z = 34.0; toleransi = 0.25;
lebar_lidah = 1.8; tinggi_lidah = 2.5; tebal_bibir = 1.8; tinggi_bibir_atas = 1.6;
tinggi_alur_total = tinggi_lidah + toleransi + tinggi_bibir_atas;
lubang_keluar = 8.0; tebal_dinding = 2.6; tinggi_kolong_bawah = 6.8; tebal_pembatas_bawah = 2.2;

mg90s_bodi_x = 12.2; mg90s_bodi_y = 22.8; mg90s_kuping_y = 32.6;
mg90s_pitch_baut = 27.6; mg90s_offset_poros = 5.6; mg90s_diameter_baut = 1.8;
servo_clearance = 0.25; tinggi_baut_ke_poros = 8.0;

posisi_poros_x = 9.5; posisi_poros_y = 12.0;
posisi_lubang_x = 39.0; posisi_lubang_y = 11.0; batas_kiri_corong_x = 18.5;
z_mulai_corong_global = 42.0; lantai_alur_z = tinggi_bawah_z - tinggi_alur_total;

module profil_corong_global() {
    rongga_penuh_x = panjang_x - (2 * tebal_dinding);
    rongga_penuh_y = lebar_y - (2 * tebal_dinding);
    lebar_corong_tengah_x = (panjang_x - tebal_dinding) - batas_kiri_corong_x;

    hull() {
        translate([tebal_dinding, tebal_dinding, z_mulai_corong_global])
            cube([rongga_penuh_x, rongga_penuh_y, 0.1]);
        translate([batas_kiri_corong_x, tebal_dinding, lantai_alur_z + 2.0])
            cube([lebar_corong_tengah_x, rongga_penuh_y, 0.1]);
    }
    hull() {
        translate([batas_kiri_corong_x, tebal_dinding, lantai_alur_z + 2.0])
            cube([lebar_corong_tengah_x, rongga_penuh_y, 0.1]);
        translate([posisi_lubang_x - (lubang_keluar / 2), posisi_lubang_y - (lubang_keluar / 2), tinggi_kolong_bawah])
            cube([lubang_keluar, lubang_keluar, 0.1]);
    }
    translate([posisi_lubang_x - (lubang_keluar / 2), posisi_lubang_y - (lubang_keluar / 2), tinggi_kolong_bawah - 1])
        cube([lubang_keluar, lubang_keluar, 2.0]);
}

lebar_alur_lidah = lebar_y + (2 * lebar_lidah) + (2 * toleransi);
lebar_total_laci = lebar_alur_lidah + (2 * tebal_bibir);
offset_y_laci = -(lebar_total_laci - lebar_y) / 2;
lantai_kuping_z = tinggi_kolong_bawah + tinggi_baut_ke_poros;

pusat_bodi_y = posisi_poros_y - mg90s_offset_poros + (mg90s_bodi_y / 2);
posisi_baut_1_y = pusat_bodi_y - (mg90s_pitch_baut / 2);
posisi_baut_2_y = pusat_bodi_y + (mg90s_pitch_baut / 2);

lebar_kantong_x = mg90s_bodi_x + (2 * servo_clearance);
panjang_kantong_y = mg90s_bodi_y + (2 * servo_clearance);
panjang_kuping_y = mg90s_kuping_y + (2 * servo_clearance);

difference() {
    translate([0, offset_y_laci, 0]) cube([panjang_x, lebar_total_laci, tinggi_bawah_z]);
    translate([-1, -lebar_lidah - toleransi, lantai_alur_z])
        cube([panjang_x - tebal_dinding + 1, lebar_alur_lidah, tinggi_lidah + toleransi]);
    translate([-1, -toleransi, lantai_alur_z])
        cube([panjang_x - tebal_dinding + 1, lebar_y + (2 * toleransi), tinggi_alur_total + 1]);
    translate([tebal_pembatas_bawah, offset_y_laci + tebal_pembatas_bawah, -0.5])
        cube([panjang_x - (2 * tebal_pembatas_bawah), lebar_total_laci - (2 * tebal_pembatas_bawah), tinggi_kolong_bawah + 0.5]);
    profil_corong_global();
    translate([posisi_poros_x - lebar_kantong_x / 2, pusat_bodi_y - panjang_kuping_y / 2, lantai_kuping_z])
        cube([lebar_kantong_x, panjang_kuping_y, tinggi_bawah_z]);
    translate([posisi_poros_x - lebar_kantong_x / 2, pusat_bodi_y - panjang_kantong_y / 2, lantai_kuping_z - 4.5])
        cube([lebar_kantong_x, panjang_kantong_y, 5.0]);
    hull() {
        translate([posisi_poros_x, posisi_poros_y, tinggi_kolong_bawah - 0.5]) cylinder(h = 5.0, d = 12.4);
        translate([posisi_poros_x, posisi_poros_y + 4.5, tinggi_kolong_bawah - 0.5]) cylinder(h = 5.0, d = 6.5);
    }
    translate([posisi_poros_x, posisi_baut_1_y, lantai_kuping_z - 7.5]) cylinder(h = 8.0, d = mg90s_diameter_baut);
    translate([posisi_poros_x, posisi_baut_2_y, lantai_kuping_z - 7.5]) cylinder(h = 8.0, d = mg90s_diameter_baut);
    translate([-1, posisi_baut_1_y - 2.5, lantai_alur_z - 3.2])
        cube([posisi_poros_x + 2, 6.0, tinggi_alur_total + 4.5]);
}
