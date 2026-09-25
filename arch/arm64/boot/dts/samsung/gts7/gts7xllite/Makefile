dtbo-y += gts7xllite_eur_openx_w00_r05.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
gts7xllite_eur_openx_w00_r05.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += gts7xllite_eur_openx_w00_r04.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
gts7xllite_eur_openx_w00_r04.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += gts7xllite_eur_openx_w00_r03.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
gts7xllite_eur_openx_w00_r03.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += gts7xllite_eur_openx_w00_r02.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
gts7xllite_eur_openx_w00_r02.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtbo-y += gts7xllite_eur_openx_w00_r00.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
gts7xllite_eur_openx_w00_r00.dtbo-base := ../../../vendor/qcom/lagoon.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
# SPDX-License-Identifier: GPL-2.0

# SPU: sec-system-update-Overlay gehoert ins dtbo.img (Stock-Entry 0)
dtbo-y += ../../lagoon-sec-system-update-overlay.dtbo
../../lagoon-sec-system-update-overlay.dtbo-base := ../../../vendor/qcom/lagoon.dtb

# lagoon.dtb braucht __symbols__ (dtc -@), damit der Bootloader die
# HW-Rev-Overlays (__fixups__) aufloesen kann — identisch zu Stock.
DTC_FLAGS_lagoon := -@

clean-files := *.dtb *.reverse.dts *.dtbo

