$fn = 60;

panjang_x = 50.0; lebar_y = 50.0; tebal_tutup = 3.0;
tebal_dinding = 2.6; toleransi = 0.25;

lebar_leher_tutup = lebar_y - (2 * tebal_dinding) - (2 * toleransi);
lebar_lidah_tutup = lebar_leher_tutup + (2 * 1.3);
panjang_tutup = panjang_x - tebal_dinding - toleransi;

// Posisi sudah dibalik (sisi atas menempel di Z = 0) agar minim support saat print!
translate([0, lebar_y, tebal_tutup])
rotate([180, 0, 0])
union() {
    cube([panjang_x, lebar_y, tebal_tutup]);
    translate([tebal_dinding + toleransi, (lebar_y - lebar_leher_tutup) / 2, -2.4])
        cube([panjang_tutup - tebal_dinding, lebar_leher_tutup, 2.4]);
    translate([tebal_dinding + toleransi, (lebar_y - lebar_lidah_tutup) / 2, -2.4])
        cube([panjang_tutup - tebal_dinding, lebar_lidah_tutup, 1.2]);
    translate([6, (lebar_y - 16) / 2, tebal_tutup])
        cube([3.5, 16, 4.0]);
}
