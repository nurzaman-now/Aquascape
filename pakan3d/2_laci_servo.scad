$fn = 60;

panjang_x = 50.0; lebar_y = 50.0; tinggi_bawah_z = 34.0; toleransi = 0.25;
lebar_lidah = 1.5; tinggi_lidah = 2.5; tebal_bibir = 1.5; tinggi_bibir_atas = 1.6;
tinggi_alur_total = tinggi_lidah + toleransi + tinggi_bibir_atas;
lubang_keluar = 8.0; tebal_dinding = 2.6; tinggi_kolong_bawah = 6.8; tebal_pembatas_bawah = 2.2;

mg90s_bodi_x = 12.2; mg90s_bodi_y = 22.8; mg90s_kuping_y = 32.6;
mg90s_pitch_baut = 27.6; mg90s_offset_poros = 5.6; mg90s_diameter_baut = 2.0;
servo_clearance = 0.35; tinggi_baut_ke_poros = 8.0;

posisi_poros_x = 9.5; posisi_poros_y = 12.0;
posisi_lubang_x = 39.0; posisi_lubang_y = 11.0; batas_kiri_corong_x = 18.5;
z_mulai_corong_global = 42.0; lantai_alur_z = tinggi_bawah_z - tinggi_alur_total;

lebar_rel = tebal_bibir + lebar_lidah + toleransi; // 3.25 mm
tebal_dinding_samping_corong = 2.0;
corong_y_min = lebar_rel + tebal_dinding_samping_corong; // 5.25 mm (mencegah tembus ke rel)
corong_lebar_y = lebar_y - (2 * corong_y_min); // 39.5 mm

// Parameter Klem Penjepit Kaca Slip-On (Solid Tanpa Baut/Lubang)
celah_kaca = 8.5;            // Lebar celah untuk kaca akuarium 8 mm (+ toleransi geser 0.5 mm)
tinggi_klem = 30.0;          // Panjang dinding penjepit ke bawah
tebal_klem_luar_atas = 6.0;  // Ketebalan dinding luar klem atas (6.0 mm)
tebal_klem_luar_bawah = 4.0; // Ketebalan dinding luar klem bawah (solid 4.0 mm)

tebal_sirip_segitiga = 5.0;  // Tebal sirip segitiga di pinggiran depan & belakang

y_klem_luar = lebar_y + celah_kaca;
y_klem_ujung = y_klem_luar + tebal_klem_luar_atas;

module profil_corong_global() {
    rongga_penuh_x = panjang_x - (2 * tebal_dinding);
    rongga_penuh_y = lebar_y - (2 * tebal_dinding);
    lebar_corong_tengah_x = (panjang_x - tebal_dinding) - batas_kiri_corong_x;

    // Corong melandai di sumbu X dan Y agar dinding samping rel tetap tebal & tidak bolong
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

lantai_kuping_z = tinggi_kolong_bawah + tinggi_baut_ke_poros;

pusat_bodi_y = posisi_poros_y - mg90s_offset_poros + (mg90s_bodi_y / 2);
posisi_baut_1_y = pusat_bodi_y - (mg90s_pitch_baut / 2);
posisi_baut_2_y = pusat_bodi_y + (mg90s_pitch_baut / 2);

lebar_kantong_x = mg90s_bodi_x + (2 * servo_clearance);
panjang_kantong_y = mg90s_bodi_y + (2 * servo_clearance);
panjang_kuping_y = mg90s_kuping_y + (2 * servo_clearance);

// Profil 2D segitiga bawah penopang
module profil_segitiga_bawah_2d() {
    polygon(points = [
        [0, 0],
        [lebar_y, 0],
        [lebar_y, -tinggi_klem]
    ]);
}

module laci_servo() {
    difference() {
        union() {
            // Kotak laci servo utama
            cube([panjang_x, lebar_y, tinggi_bawah_z]);

            // 1. Sirip Segitiga Bawah HANYA di Pinggiran (Kiri & Kanan), bagian tengah terbuka untuk akses katup
            // Sirip pinggir depan (X = 0 s/d tebal_sirip_segitiga)
            rotate([90, 0, 90])
            linear_extrude(height = tebal_sirip_segitiga)
                profil_segitiga_bawah_2d();

            // Sirip pinggir belakang (X = panjang_x - tebal_sirip_segitiga s/d panjang_x)
            translate([panjang_x - tebal_sirip_segitiga, 0, 0])
            rotate([90, 0, 90])
            linear_extrude(height = tebal_sirip_segitiga)
                profil_segitiga_bawah_2d();

            // Dinding penahan dalam yang bersandar ke kaca akuarium (Y = lebar_y - 2.5 s/d lebar_y)
            translate([0, lebar_y - 2.5, -tinggi_klem])
                cube([panjang_x, 2.5, tinggi_klem]);

            // 2. Segitiga atas pengikat klem luar ke bodi laci
            rotate([90, 0, 90])
            linear_extrude(height = panjang_x)
                polygon(points = [
                    [lebar_y, 0],
                    [lebar_y, tinggi_bawah_z],
                    [y_klem_ujung, 0]
                ]);

            // 3. Dinding klem luar tebal solid (Murni jepit tanpa baut/lubang)
            rotate([90, 0, 90])
            linear_extrude(height = panjang_x)
                polygon(points = [
                    [y_klem_luar, 0],
                    [y_klem_ujung, 0],
                    [y_klem_luar + tebal_klem_luar_bawah, -tinggi_klem],
                    [y_klem_luar, -tinggi_klem]
                ]);
        }

        // Rel alur kiri (terbuka di X = 50, stopper di dinding kanan X = 0 s/d tebal_dinding)
        translate([tebal_dinding, tebal_bibir, lantai_alur_z])
            cube([panjang_x - tebal_dinding + 1, lebar_lidah + toleransi, tinggi_lidah + toleransi]);

        // Rel alur kanan (terbuka di X = 50, stopper di dinding kanan X = 0 s/d tebal_dinding)
        translate([tebal_dinding, lebar_y - tebal_bibir - (lebar_lidah + toleransi), lantai_alur_z])
            cube([panjang_x - tebal_dinding + 1, lebar_lidah + toleransi, tinggi_lidah + toleransi]);

        // Rongga tengah atas dipotong rapi dari tebal_dinding sampai ujung kiri X = 50 (tembus keluar)
        translate([tebal_dinding, lebar_rel, lantai_alur_z])
            cube([panjang_x - tebal_dinding + 1, lebar_y - (2 * lebar_rel), tinggi_alur_total + 0.1]);



        // Kolong bawah untuk katup horn berputar (terbuka leluasa dari bawah)
        translate([tebal_pembatas_bawah, tebal_pembatas_bawah, -0.5])
            cube([panjang_x - (2 * tebal_pembatas_bawah), lebar_y - (2 * tebal_pembatas_bawah), tinggi_kolong_bawah + 0.5]);

        profil_corong_global();

        // Kantong servo presisi MG90S
        // 1. Rongga atas untuk kuping baut servo (seukuran dan simetris presisi antara lubang baut atas dan bawah)
        translate([posisi_poros_x - lebar_kantong_x / 2 - 0.45, pusat_bodi_y - panjang_kuping_y / 2, lantai_kuping_z])
            cube([lebar_kantong_x + 0.45, panjang_kuping_y, tinggi_bawah_z]);

        // 2. Dudukan bodi bawah servo (5.5 mm di bawah lantai kuping agar kuping duduk rata sempurna)
        translate([posisi_poros_x - lebar_kantong_x / 2, pusat_bodi_y - panjang_kantong_y / 2, lantai_kuping_z - 5.5])
            cube([lebar_kantong_x, panjang_kantong_y, 6.0]);

        // 3. Lubang leher gir dan poros MG90S tembus bersih ke kolong katup
        hull() {
            translate([posisi_poros_x, posisi_poros_y, tinggi_kolong_bawah - 1.0])
                cylinder(h = tinggi_baut_ke_poros + 2.0, d = 12.6);
            translate([posisi_poros_x, posisi_poros_y + 4.5, tinggi_kolong_bawah - 1.0])
                cylinder(h = tinggi_baut_ke_poros + 2.0, d = 7.0);
        }

        // 4. Lubang sekrup kuping MG90S (diameter 2.0 mm, kedalaman 8.0 mm pas dengan sekrup bawaan)
        translate([posisi_poros_x, posisi_baut_1_y, lantai_kuping_z - 8.0])
            cylinder(h = 8.5, d = mg90s_diameter_baut);
        translate([posisi_poros_x, posisi_baut_2_y, lantai_kuping_z - 8.0])
            cylinder(h = 8.5, d = mg90s_diameter_baut);

        // 5. Jalur kabel servo di sebelah kiri (Y = 8.0 mm) dangkal rapi (tidak terlalu dalam)
        lebar_kabel_mg90s = 2.0;
        y_posisi_kabel = 2.15; // Di sebelah kiri poros gir (antara baut 1 di Y=4 dan poros di Y=12)
        kedalaman_kabel_dari_rel = 3.0; // Cukup 2.5 mm di bawah lantai rel agar tidak terlalu dalam
        translate([-0.5, y_posisi_kabel - (lebar_kabel_mg90s / 2), lantai_alur_z - kedalaman_kabel_dari_rel])
            cube([tebal_dinding + 1.5, lebar_kabel_mg90s, tinggi_alur_total + kedalaman_kabel_dari_rel + 0.5]);
    }
}

laci_servo();
