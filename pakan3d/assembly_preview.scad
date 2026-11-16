$fn = 60;

use <1_kotak_atas.scad>
use <2_laci_servo.scad>
use <3_tutup_slide.scad>
use <4_katup_horn.scad>

// Parameter Simulasi:
// Sudut bukaan katup (0 = tertutup rapat, 45 = terbuka 100%)
sudut_buka_katup = 45; // Ganti ke 45 untuk melihat saat pakan jatuh

// Tampilkan kaca akuarium transparan (tebal 8 mm pas di dalam celah klem)
translate([-10, 50, -50])
    color([0.3, 0.75, 0.9, 0.35])
    cube([70, 8.0, 50]);

// 1. Laci Servo Bawah + Klem Kaca Slip-On (Abu-abu Cerah)
color([0.85, 0.85, 0.88])
    laci_servo();

// 2. Kotak Penampung Pakan Atas (Biru Langit Transparan)
// Terpasang pas rapat pada alur rel laci servo di Z = 29.65 mm
translate([0, 0, 29.65])
    color([0.2, 0.55, 0.9, 0.75])
    kotak_atas();

// 3. Tutup Geser Atas (Oranye)
translate([0, 0, 97.0])
    color([0.95, 0.55, 0.15])
    tutup_slide(siap_print = false);

// 4. Katup Horn Servo (Kuning Emas)
// Terpasang di gir poros servo di (9.5, 12.0) pada kolong bawah
sudut_dasar_katup = atan2(11.0 - 12.0, 39.0 - 9.5);
translate([9.5, 12.0, 3.2])
rotate([0, 0, sudut_dasar_katup + sudut_buka_katup])
    color([0.95, 0.85, 0.15])
    katup_horn(siap_print = false);
