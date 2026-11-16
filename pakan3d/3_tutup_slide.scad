
$fn = 60;

panjang_x = 50.0; lebar_y = 50.0; tebal_tutup = 3.0;
tebal_dinding = 2.6; toleransi = 0.25;

lebar_leher_tutup = lebar_y - (2 * tebal_dinding) - (2 * toleransi);
lebar_lidah_tutup = lebar_leher_tutup + (2 * 1.3);
panjang_tutup = panjang_x - tebal_dinding - toleransi;

// Tutup Slide Datar Rapi (100% Rata Tanpa Tonjolan Handle)
module bentuk_tutup_dasar() {
    difference() {
        union() {
            // Pelat tutup utama rata sempurna
            cube([panjang_x, lebar_y, tebal_tutup]);

            // Lidah rel geser di bawah pelat
            translate([0, (lebar_y - lebar_leher_tutup) / 2, -2.4])
                cube([panjang_tutup, lebar_leher_tutup, 2.4]);
            translate([0, (lebar_y - lebar_lidah_tutup) / 2, -2.4])
                cube([panjang_tutup, lebar_lidah_tutup, 1.2]);
        }

        // Ceruk jari halus ke dalam (recessed grip) sedalam 1.0 mm
        // Permukaan atas tetap 100% rata, tidak ada yang menonjol keluar
        translate([7.0, (lebar_y - 22) / 2, tebal_tutup - 1.0])
            cube([4.0, 22.0, 1.2]);
    }
}

module tutup_slide(siap_print = true) {
    if (siap_print) {
        translate([0, lebar_y, tebal_tutup])
        rotate([180, 0, 0])
            bentuk_tutup_dasar();
    } else {
        bentuk_tutup_dasar();
    }
}

tutup_slide(siap_print = false);
