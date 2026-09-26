# Kernel-Delta gts7xllite (SM-T736B) — Port auf 4.19.325

Quelle: `kernel/samsung/sm7225_old` (Samsung-Dump 4.19.152, T736BXXS9DYF1)
Ziel:   dieser Baum (TARKZiM-Basis 4.19.325, a42xq-Port bereits enthalten)
Branch: `gts7xllite-23.2`

## A. DTS / Boot

- [x] `arch/arm64/boot/dts/samsung/gts7/` kopiert (5 Overlay-DTS + Makefile)
- [x] `dts/samsung/Makefile`: `dts-dirs += gts7/gts7xllite` im GTS7XLLITE-Branch
- [x] `dts/vendor/qcom/Makefile`: `DTC_FLAGS_lagoon := -@`
- [x] `lagoon.dtsi` reserved-mem-Diff geprueft/uebernommen
- [x] `vendor/gts7xllite_eur_openx_defconfig` eingefuehrt, Symbole abgeglichen
- [x] Firmware-Blobs + `firmware/Makefile` (tsp_focaltech, keyboard_stm, tsp_slsi, w9021, hx83102e-init)
- [x] UH/RKP/KDP komplett aus (wie a42xq-Referenz; KDP_NS-fs/-Umbau nicht portiert)

## B. Display

- [x] `techpack/display/msm/samsung/{FT8203_TS124QDM,HX83102_TV104WUM,HX83121_PPC357DB11}` kopiert
- [x] `techpack/display/msm/samsung/{BLIC,PMIC}` kopiert
- [x] `panel_common_conf.h` + `ss_wrapper_common.c/h` kopiert
- [x] `techpack/display/msm/Makefile` obj-Zeilen
- [x] `ss_dsi_panel_common.c` FT8203-Init-Hook + Delta-Review
- [x] Root-`Kconfig` Panel-source-Zeilen
- [x] `samsung/`-Subtree auf gts7xl-Generation ersetzt; `sec_displayport.h` SECDP_SWITCH-Define nachgezogen

## C. Input

- [x] `drivers/input/sec_input/` (sec_input.h, sec_common_fn.c, sec_input_notifier.c, Kconfig, Makefile; dedup vs input/common/)
- [x] `drivers/input/sec_input/stm32/` (Pogo) + `include/linux/input/pogo_i2c_notifier.h`
- [x] `drivers/input/touchscreen/focaltech/` (ft820x) — war bereits vollstaendig
- [x] `drivers/input/input_boost/` + `include/linux/input/input_booster.h`
- [x] `drivers/input/misc/hall/` (hall_ic_logical, hall_ic_notifier, sec_hall_dumpkey) + `include/linux/hall/`
- [x] himax: gts7xl-DTS-Props gemergt (area-size, vendor_check-gpio, notify_tsp_esd)
- [x] himax HX83121A: Stock-FW 255K (ver 01010d) + FW_SIZE_255k/_255k-Routine + Layout-Adressen 0x214xx/0x215xx + 2nd-flash-reload-Reset in read_FW_ver; falscher 128K-Blob (ver 010183) ersetzt — Touch verifiziert (ver IC=01010D, reload OK, echte Koordinaten)
- [x] himax HX83121A Finger-Drag-Fix: `HX_NEW_EVENT_STACK_FORMAT` + `SEC_PALM_FUNC` + `SEC_FINGER_INFO_SZ=24` portiert — Stock-FW sendet 80B-Frames (56B Info + 24B maj/min-Trailer, Trailer-Checksum @79); 56B-Reads erzeugten Phantom-Release-Frames (point_num=0 alle ~150 ms → Tap-Serie statt Drag). Jetzt: 80B-Read pro IRQ, maj/min via `coordInfoSize+i*2`, `ABS_MT_TOUCH_MAJOR/MINOR`, `BTN_PALM`, palm_flag. Verifiziert: kontinuierliche Kontakte (mc bis 306, dd >1000px), Scrollen funktioniert. Debug-Helfer: tsdbg1 liest 128B + RAW-Dump.
- [x] LEGO-Zeilen in `drivers/Kconfig` + `drivers/Makefile`
- [x] Wacom wez01/Firmware verifiziert
- [x] input/common-Guard: `ifeq (,$(CONFIG_INPUT_SEC_INPUT))` gegen doppelte Symbole

## D. Power

- [x] `drivers/battery/` gts7xl-Snapshot (sm5440_charger, sec_direct_charger, battery_logger, sb_full_soc, sec_*_sysfs, sec_battery*, sm5714_*, dtsi)
- [x] `drivers/muic/` muic_param.c + muic_sysfs.c + sm5714-MUIC + include/linux/muic/common/
- [x] `drivers/usb/typec/common/pdic_*.c` + include-Header
- [x] `drivers/usb/gadget/function/f_ss_mon_gadget.c`
- [x] `drivers/regulator/s2mpb03/` + include/linux/regulator/s2mpb03.h
- [x] sysfs-Pflichtknoten verifiziert (batt_slate_mode, afc_disable)
- [x] `sec-battery.h`: SIOP_DEFAULT / SIOP_SCENARIO_VT_CALL nachgezogen
- [x] `ps5169.h`: is_DFP + ps5169_notify_dplink nachgezogen
- [x] `usb_notify.h` volle gts7xl-Generation; `usb_hw_param.h` USB_HOST_*-Eintraege
- [x] `dwc3-msm.c`: is_dwc3_msm_probe_done() portiert

## E. Sensoren / ADSP

- [x] `drivers/sensors/`: isg6320.c/_reg.h, Kconfig-Optionen, a96t3x6*/sx9360*-Merge
- [x] `drivers/adsp_factory/`: stk3a9x_light/prox, ssc_core.c/adsp.h/stk31610/flip_cover/veml3xxx-Merge
- [x] `adsp_ft_common.h`: OPTION_TYPE_SSC_* Enum-Reihenfolge Stock-konform

## F. Audio

- [x] `sound/soc/codecs/`: bigdata_cs35l45_sysfs_cb.*, cs35l45_dsp_events.h
- [x] `techpack/audio/` auf gts7xl-Generation (dbmdx vorhanden); PROJECT_NAME=lito_gts7plite via gts7xllite

## G. Kamera

- [x] hi1336/cam_eeprom: kein gts7xl-relevantes Delta — uebersprungen

## H. Rest

- [x] `drivers/samsung/lmkd_debug.c`
- [x] `drivers/hwmon/sec_thermistor/sec_thermistor.h`
- [x] `drivers/samsung/{debug,misc,quest}/` gts7xl-Delta
- [x] `security/samsung/` gts7xl-Generation (defex_lsm inkl. refcount/variadic-Fixes)
- [x] `drivers/leds/leds-sm5714-fled.c`
- [x] wcn39xx gts7xl-Stand uebernommen (Android-U-Fixes der Basis verifiziert)
- [x] `clk.c` doppeltes all_lists entfernt; `pgalloc.h` stray else unter CONFIG_RKP gefixt
- [x] `cred.c`/`defex`: atomic_long_t/refcount_t-Anpassungen auf 4.19.325
- [x] `rbincache.c`: rc_sysfs_init ohne __init (Link-Section-Mismatch)
- [x] Build-Sichtbarkeit: leere `Android.mk` in `sm7225_old/` (Scan-Shield)

## Verifikation

- [x] `m kernel` fehlerfrei (vmlinux + 7 Module)
- [x] `m dtboimage` → 6 DTBO-Entries (sec-system-update + r00/r02/r03/r04/r05)
- [x] `m bootimage` → boot.img (100 MB, ANDROID!-Magic)
- [x] vendor/lib/modules: llcc_perfmon, mpq-adapter, mpq-dmx-hw-plugin, rdbg, rmnet_perf, rmnet_shs (+mmc_test, tcp_htcp, tcp_westwood)
- [ ] Boottest (User): Display, Touch ok, S-Pen ok, Pogo ok (EF-DT730 erkannt, Key-Events sauber), Charge/AFC ok (PD-APDO ~9V, sm5440 Direct-Charge, +2.3A in Akku), Audio ok (Lautsprecher, seit C2-HAL-Fix), Cam, Sensoren ok (Autorotation; lsm6dso/ak0991x/VEML3235/A96T3X6 + Samsung-Virtuals aktiv), 5G(LTE laeuft), WLAN ok (kein NFC-Modul in diesem Geraet)

## Vendor-Fixes (vendor/samsung/sm7225-common, Commit 1a511ba)

- [x] Browser-Haenger (gmx.net/google.de lud nicht fertig): samsung-software-media-c2-hal-1-0 crash-looopte (SIGSYS) — seccomp-Policy blockierte mremap(MREMAP_MAYMOVE). `mremap: 1` in samsung.software.media.c2-base-policy. WebView blockierte auf IComponentStore/default bei Video-Elementen.
- [x] vendor.samsung.hardware.media.converter@1.0-service Linker-Loop: patchelf NEEDED `common-V1-ndk_platform.so` → `-V2-ndk.so` (platform-Varianten existieren auf A16 nicht mehr)
