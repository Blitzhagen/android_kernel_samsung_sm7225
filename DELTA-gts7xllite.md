# Kernel-Delta gts7xllite (SM-T736B) — Port auf 4.19.325

Quelle: `kernel/samsung/sm7225_old` (Samsung-Dump 4.19.152, T736BXXS9DYF1)
Ziel:   dieser Baum (TARKZiM-Basis 4.19.325, a42xq-Port bereits enthalten)
Branch: `gts7xllite-23.2`

## A. DTS / Boot

- [ ] `arch/arm64/boot/dts/samsung/gts7/` kopiert (5 Overlay-DTS + Makefile)
- [ ] `dts/samsung/Makefile`: `dts-dirs += gts7/gts7xllite` im GTS7XLLITE-Branch
- [ ] `dts/vendor/qcom/Makefile`: `DTC_FLAGS_lagoon := -@`
- [ ] `lagoon.dtsi` reserved-mem-Diff geprueft/uebernommen
- [ ] `vendor/gts7xllite_eur_openx_defconfig` eingefuehrt, Symbole abgeglichen
- [ ] Firmware-Blobs + `firmware/Makefile` (tsp_focaltech, keyboard_stm, tsp_slsi, w9021, hx83102e-init)

## B. Display

- [ ] `techpack/display/msm/samsung/{FT8203_TS124QDM,HX83102_TV104WUM,HX83121_PPC357DB11}` kopiert
- [ ] `techpack/display/msm/samsung/{BLIC,PMIC}` kopiert
- [ ] `panel_common_conf.h` + `ss_wrapper_common.c/h` kopiert
- [ ] `techpack/display/msm/Makefile` obj-Zeilen
- [ ] `ss_dsi_panel_common.c` FT8203-Init-Hook + Delta-Review
- [ ] Root-`Kconfig` Panel-source-Zeilen

## C. Input

- [ ] `drivers/input/sec_input/` (sec_input.h, sec_common_fn.c, sec_input_notifier.c, Kconfig, Makefile; dedup vs input/common/)
- [ ] `drivers/input/sec_input/stm32/` (Pogo) + `include/linux/input/pogo_i2c_notifier.h`
- [ ] `drivers/input/touchscreen/focaltech/` (ft820x)
- [ ] `drivers/input/input_boost/` + `include/linux/input/input_booster.h`
- [ ] `drivers/input/misc/hall/` (hall_ic_logical, hall_ic_notifier, sec_hall_dumpkey) + `include/linux/hall/`
- [ ] himax: gts7xl-DTS-Props in himax_83102E verifiziert/gemergt
- [ ] LEGO-Zeilen in `drivers/Kconfig` + `drivers/Makefile`
- [ ] Wacom wez01/Firmware verifiziert

## D. Power

- [ ] `drivers/battery/` gts7xl-Snapshot (sm5440_charger, sec_direct_charger, battery_logger, sb_full_soc, sec_*_sysfs, sec_battery*, sm5714_*, dtsi)
- [ ] `drivers/muic/` muic_param.c + muic_sysfs.c + sm5714-MUIC + include/linux/muic/common/
- [ ] `drivers/usb/typec/common/pdic_*.c` + include-Header
- [ ] `drivers/usb/gadget/function/f_ss_mon_gadget.c`
- [ ] `drivers/regulator/s2mpb03/` + include/linux/regulator/s2mpb03.h
- [ ] sysfs-Pflichtknoten verifiziert (batt_slate_mode, afc_disable)

## E. Sensoren / ADSP

- [ ] `drivers/sensors/`: isg6320.c/_reg.h, Kconfig-Optionen, a96t3x6*/sx9360*-Merge
- [ ] `drivers/adsp_factory/`: stk3a9x_light/prox, ssc_core.c/adsp.h/stk31610/flip_cover/veml3xxx-Merge

## F. Audio

- [ ] `sound/soc/codecs/`: bigdata_cs35l45_sysfs_cb.*, cs35l45_dsp_events.h
- [ ] `techpack/audio/` Content-Diffs gereviewt (dbmdx vorhanden)

## G. Kamera

- [ ] `techpack/camera/.../cam_eeprom/hi1336_otp.h` + cam_eeprom/cam_sensor-Diffs
- [ ] SAMSUNG_CAMERA_OTP_FRONT / SUPPORT_*_OPTIC Configs

## H. Rest

- [ ] `drivers/samsung/lmkd_debug.c`
- [ ] `drivers/hwmon/sec_thermistor/sec_thermistor.h`
- [ ] `drivers/samsung/{debug,misc,quest}/` gts7xl-Delta wo noetig
- [ ] `security/samsung/five/{gki,s_os}` u. a. falls defconfig verlangt
- [ ] `drivers/leds/leds-sm5714-fled.c` Review
- [ ] wcn39xx-GPIO-Teile bewertet

## Verifikation

- [ ] `m kernel` fehlerfrei
- [ ] `m dtboimage` → 6 DTBO-Entries (sec-system-update + r00/r02/r03/r04/r05)
- [ ] `m bootimage` → boot.img
- [ ] vendor/lib/modules: llcc_perfmon, mpq-adapter, mpq-dmx-hw-plugin, rdbg, rmnet_perf, rmnet_shs
- [ ] Boottest (User): Display, Touch, S-Pen, Pogo, Charge/AFC, Audio, Cam, Sensoren, 5G, WLAN, NFC
