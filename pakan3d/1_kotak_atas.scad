$fn = 60;

panjang_x = 50.0; lebar_y = 50.0; tinggi_total_10cm = 100.0;
tinggi_bawah_z = 34.0; tebal_tutup = 3.0; toleransi = 0.25;
lebar_lidah = 1.5; tinggi_lidah = 2.5; tebal_bibir = 1.5; tinggi_bibir_atas = 1.6;
tinggi_alur_total = tinggi_lidah + toleransi + tinggi_bibir_atas;
tinggi_atas_z = (tinggi_total_10cm - tinggi_bawah_z - tebal_tutup) + tinggi_alur_total;
lubang_keluar = 8.0; tebal_dinding = 2.6; tinggi_kolong_bawah = 6.8;
posisi_lubang_x = 39.0; posisi_lubang_y = 11.0; batas_kiri_corong_x = 18.5;
z_mulai_corong_global = 42.0; lantai_alur_z = tinggi_bawah_z - tinggi_alur_total;

lebar_rel = tebal_bibir + lebar_lidah + toleransi; // 3.25 mm
tebal_dinding_samping_corong = 2.0;
corong_y_min = lebar_rel + tebal_dinding_samping_corong; // 5.25 mm
corong_lebar_y = lebar_y - (2 * corong_y_min); // 39.5 mm

module profil_corong_global() {
    rongga_penuh_x = panjang_x - (2 * tebal_dinding);
    rongga_penuh_y = lebar_y - (2 * tebal_dinding);
    lebar_corong_tengah_x = (panjang_x - tebal_dinding) - batas_kiri_corong_x;

    hull() {
        translate([tebal_dinding, tebal_dinding, z_mulai_corong_global])
            cube([rongga_penuh_x, rongga_penuh_y, 0.1]);
        translate([batas_kiri_corong_x, corong_y_min, lantai_alur_z + 2.0])
            cube([lebar_corong_tengah_x, corong_lebar_y, 0.1]);
    }
    hull() {
        translate([batas_kiri_corong_x, corong_y_min, lantai_alur_z + 2.0])
            cube([lebar_corong_tengah_x, corong_lebar_y, 0.1]);
        translate([posisi_lubang_x - (lubang_keluar / 2), posisi_lubang_y - (lubang_keluar / 2), tinggi_kolong_bawah])
            cube([lubang_keluar, lubang_keluar, 0.1]);
    }
    translate([posisi_lubang_x - (lubang_keluar / 2), posisi_lubang_y - (lubang_keluar / 2), tinggi_kolong_bawah - 1])
        cube([lubang_keluar, lubang_keluar, 2.0]);
}

lebar_alur_tutup = lebar_y - (2 * tebal_dinding);
lebar_lidah_tutup = lebar_alur_tutup + (2 * 1.4);
z_mulai_lokal = z_mulai_corong_global - lantai_alur_z;

module kotak_atas() {
    difference() {
        cube([panjang_x, lebar_y, tinggi_atas_z]);

        // 1. Celah luar presisi untuk dinding luar laci servo
        translate([-1, -1, -0.1])
            cube([panjang_x + 2, tebal_bibir + toleransi + 1, tinggi_alur_total + 0.1]);
        translate([-1, lebar_y - (tebal_bibir + toleransi), -0.1])
            cube([panjang_x + 2, tebal_bibir + toleransi + 1, tinggi_alur_total + 0.1]);

        // 2. Celah di atas lidah presisi untuk bibir penahan laci servo
        translate([-1, -1, tinggi_lidah])
            cube([panjang_x + 2, lebar_rel + 1, tinggi_alur_total - tinggi_lidah]);
        translate([-1, lebar_y - lebar_rel, tinggi_lidah])
            cube([panjang_x + 2, lebar_rel + 1, tinggi_alur_total - tinggi_lidah]);

        // 3. Potongan stopper dibalik ke depan (X = 0 s/d tebal_dinding) agar pas dengan dinding stopper laci
        translate([-0.1, -1, -0.1])
            cube([tebal_dinding + 0.1, lebar_y + 2, tinggi_alur_total + 0.1]);

        // Ujung lidah di X = 50 dibuat rata rapi 90 derajat siku seperti rel tutup slide (Y: 0 dekat X: 0)

        // Rongga dalam kotak penampung atas
        translate([tebal_dinding, tebal_dinding, z_mulai_lokal])
            cube([panjang_x - (2 * tebal_dinding), lebar_y - (2 * tebal_dinding), tinggi_atas_z]);
        translate([0, 0, -lantai_alur_z]) profil_corong_global();

        // Rel geser untuk tutup atas
        translate([-1, (lebar_y - lebar_lidah_tutup) / 2, tinggi_atas_z - 2.6])
            cube([panjang_x - tebal_dinding + 1, lebar_lidah_tutup, 1.5]);
        translate([-1, tebal_dinding, tinggi_atas_z - 2.6])
            cube([panjang_x - tebal_dinding + 1, lebar_alur_tutup, 3.0]);
    }
}
kotak_atas();
