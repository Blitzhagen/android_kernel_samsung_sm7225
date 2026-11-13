/*
 *  Bluetooth supports for Qualcomm Atheros chips
 *
 *  Copyright (c) 2015 The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/module.h>
#include <linux/firmware.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "btqca.h"
#include "hci_uart.h"

#define VERSION "0.1"

int qca_read_soc_version(struct hci_dev *hdev, u32 *soc_version, u32 *soc_id)
{
	struct sk_buff *skb;
	struct edl_event_hdr *edl;
	struct rome_version *ver;
	char cmd;
	int err = 0;

	bt_dev_dbg(hdev, "QCA Version Request");

	cmd = EDL_PATCH_VER_REQ_CMD;
	skb = __hci_cmd_sync_ev(hdev, EDL_PATCH_CMD_OPCODE, EDL_PATCH_CMD_LEN,
				&cmd, HCI_EV_VENDOR, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		bt_dev_err(hdev, "Reading QCA version information failed (%d)",
			   err);
		return err;
	}

	/* WCN399x parts may answer with the version inside a command
	 * complete event: rtype is then the command opcode (0x19) and the
	 * version payload is preceded by a status byte, making the
	 * response one byte longer than the ROME layout. */
	if (skb->len != sizeof(*edl) + sizeof(*ver) &&
	    skb->len != sizeof(*edl) + sizeof(*ver) + 1) {
		bt_dev_err(hdev, "QCA Version size mismatch len %d", skb->len);
		err = -EILSEQ;
		goto out;
	}

	edl = (struct edl_event_hdr *)(skb->data);
	if (!edl) {
		bt_dev_err(hdev, "QCA TLV with no header");
		err = -EILSEQ;
		goto out;
	}

	if (edl->cresp != EDL_CMD_REQ_RES_EVT ||
	    (edl->rtype != EDL_APP_VER_RES_EVT &&
	     edl->rtype != EDL_PATCH_VER_REQ_CMD)) {
		bt_dev_err(hdev, "QCA Wrong packet received %d %d", edl->cresp,
			   edl->rtype);
		err = -EIO;
		goto out;
	}

	if (edl->rtype == EDL_PATCH_VER_REQ_CMD) {
		if (skb->len != sizeof(*edl) + sizeof(*ver) + 1) {
			err = -EILSEQ;
			goto out;
		}
		ver = (struct rome_version *)(edl->data + 1);
	} else {
		ver = (struct rome_version *)(edl->data);
	}

	BT_DBG("%s: Product:0x%08x", hdev->name, le32_to_cpu(ver->product_id));
	BT_DBG("%s: Patch  :0x%08x", hdev->name, le16_to_cpu(ver->patch_ver));
	BT_DBG("%s: ROM    :0x%08x", hdev->name, le16_to_cpu(ver->rome_ver));
	BT_DBG("%s: SOC    :0x%08x", hdev->name, le32_to_cpu(ver->soc_id));

	/* QCA chipset version can be decided by patch and SoC
	 * version, combination with upper 2 bytes from SoC
	 * and lower 2 bytes from patch will be used.
	 */
	*soc_version = (le32_to_cpu(ver->soc_id) << 16) |
			(le16_to_cpu(ver->rome_ver) & 0x0000ffff);
	if (soc_id)
		*soc_id = le32_to_cpu(ver->soc_id);
	if (*soc_version == 0)
		err = -EILSEQ;

out:
	kfree_skb(skb);
	if (err)
		bt_dev_err(hdev, "QCA Failed to get version (%d)", err);

	return err;
}
EXPORT_SYMBOL_GPL(qca_read_soc_version);

int qca_send_reset(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	int err, retries = 5;

	bt_dev_dbg(hdev, "QCA HCI_RESET");

	/* After NVM download the controller reboots into the new firmware.
	 * A reset sent while it is still booting is silently dropped, so
	 * retry a few times. The host baud rate stays at the NVM-programmed
	 * speed — switching it here would only desync the IBS handshake. */
	do {
		skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL,
				     HCI_INIT_TIMEOUT);
		if (!IS_ERR(skb))
			break;
		err = PTR_ERR(skb);
		if (err != -ETIMEDOUT || --retries <= 0) {
			bt_dev_err(hdev, "QCA Reset failed (%d)", err);
			return err;
		}
		bt_dev_warn(hdev, "QCA HCI_RESET timeout, retrying");
		msleep(300);
	} while (retries > 0);

	kfree_skb(skb);

	return 0;
}
EXPORT_SYMBOL_GPL(qca_send_reset);

/* Send a small vendor-specific EDL-era command and wait for its
 * command-complete. Used for the post-NVM commands the Android QTI
 * HAL issues (add-on feature list, enhanced controller log etc.)
 * before the final HCI reset boots the downloaded firmware. Errors
 * are returned to the caller but are expected to be treated as
 * non-fatal. */
int qca_send_edl_cmd(struct hci_dev *hdev, u16 opcode, u8 plen,
		     const void *param)
{
	struct sk_buff *skb;

	skb = __hci_cmd_sync(hdev, opcode, plen, (void *)param,
			     HCI_INIT_TIMEOUT);
	if (IS_ERR(skb))
		return PTR_ERR(skb);
	kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL_GPL(qca_send_edl_cmd);

int qca_read_fw_build_info(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	struct edl_event_hdr *edl;
	char cmd, build_label[QCA_FW_BUILD_VER_LEN];
	int build_lbl_len, err = 0;

	bt_dev_dbg(hdev, "QCA read fw build info");

	cmd = EDL_GET_BUILD_INFO_CMD;
	skb = __hci_cmd_sync_ev(hdev, EDL_PATCH_CMD_OPCODE, EDL_PATCH_CMD_LEN,
				&cmd, 0, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		bt_dev_err(hdev, "Reading QCA fw build info failed (%d)",
			   err);
		return err;
	}

	edl = (struct edl_event_hdr *)(skb->data);
	if (!edl) {
		bt_dev_err(hdev, "QCA read fw build info with no header");
		err = -EILSEQ;
		goto out;
	}

	if (edl->cresp != EDL_CMD_REQ_RES_EVT ||
	    edl->rtype != EDL_GET_BUILD_INFO_CMD) {
		bt_dev_err(hdev, "QCA Wrong packet received %d %d",
			   edl->cresp, edl->rtype);
		err = -EIO;
		goto out;
	}

	build_lbl_len = edl->data[0];
	if (build_lbl_len <= QCA_FW_BUILD_VER_LEN - 1) {
		memcpy(build_label, edl->data + 1, build_lbl_len);
		build_label[build_lbl_len] = '\0';
		bt_dev_info(hdev, "QCA fw build info: %s", build_label);
	}

out:
	kfree_skb(skb);
	return err;
}
EXPORT_SYMBOL_GPL(qca_read_fw_build_info);

static void qca_tlv_check_data(struct rome_config *config,
				u8 *fw_data, bool wcn3991)
{
	const u8 *data;
	u32 type_len;
	u16 tag_id, tag_len;
	int idx, length, i;
	struct tlv_type_hdr *tlv;
	struct tlv_type_patch *tlv_patch;
	struct tlv_type_nvm *tlv_nvm;

	tlv = (struct tlv_type_hdr *)fw_data;

	type_len = le32_to_cpu(tlv->type_len);
	length = (type_len >> 8) & 0x00ffffff;

	BT_DBG("TLV Type\t\t : 0x%x", type_len & 0x000000ff);
	BT_DBG("Length\t\t : %d bytes", length);

	config->dnld_mode = ROME_SKIP_EVT_NONE;

	switch (config->type) {
	case TLV_TYPE_PATCH:
		tlv_patch = (struct tlv_type_patch *)tlv->data;

		/* For Rome version 1.1 to 3.1, all segment commands
		 * are acked by a vendor specific event (VSE).
		 * For Rome >= 3.2, the download mode field indicates
		 * if VSE is skipped by the controller.
		 * In case VSE is skipped, only the last segment is acked.
		 */
		config->dnld_mode = tlv_patch->download_mode;

		BT_DBG("Total Length           : %d bytes",
		       le32_to_cpu(tlv_patch->total_size));
		BT_DBG("Patch Data Length      : %d bytes",
		       le32_to_cpu(tlv_patch->data_length));
		BT_DBG("Signing Format Version : 0x%x",
		       tlv_patch->format_version);
		BT_DBG("Signature Algorithm    : 0x%x",
		       tlv_patch->signature);
		BT_DBG("Download mode          : 0x%x",
		       tlv_patch->download_mode);
		BT_DBG("Reserved               : 0x%x",
		       tlv_patch->reserved1);
		BT_DBG("Product ID             : 0x%04x",
		       le16_to_cpu(tlv_patch->product_id));
		BT_DBG("Rom Build Version      : 0x%04x",
		       le16_to_cpu(tlv_patch->rom_build));
		BT_DBG("Patch Version          : 0x%04x",
		       le16_to_cpu(tlv_patch->patch_version));
		BT_DBG("Reserved               : 0x%x",
		       le16_to_cpu(tlv_patch->reserved2));
		BT_DBG("Patch Entry Address    : 0x%x",
		       le32_to_cpu(tlv_patch->entry));
		break;

	case TLV_TYPE_NVM:
		data = tlv->data;
		/* Samsung NVM files carry a small preamble before the
		 * tag records. Locate the real tag area by finding a
		 * start offset that yields a chain of at least two
		 * plausible records. */
		for (idx = -1, i = 0; i < 64 && idx < 0; i += 2) {
			int probe = i, ok = 0;

			while (probe + 12 <= length) {
				tlv_nvm = (struct tlv_type_nvm *)(data + probe);
				tag_id = le16_to_cpu(tlv_nvm->tag_id);
				tag_len = le16_to_cpu(tlv_nvm->tag_len);
				if (!tag_id || tag_id > 0x100 ||
				    probe + 12 + tag_len > length)
					break;
				ok++;
				probe += 12 + tag_len;
			}
			if (ok >= 2)
				idx = i;
		}
		if (idx < 0) {
			BT_ERR("QCA NVM tag area not found");
			break;
		}
		while (idx + 12 <= length) {
			tlv_nvm = (struct tlv_type_nvm *)(data + idx);

			tag_id = le16_to_cpu(tlv_nvm->tag_id);
			tag_len = le16_to_cpu(tlv_nvm->tag_len);

			/* Update NVM tags as needed */
			switch (tag_id) {
			case EDL_TAG_ID_HCI:
				/* HCI transport layer parameters.
				 * On WCN3991 (new NVM format, as verified in the
				 * QTI userspace HAL) the baudrate enum lives at
				 * data[1] and data[0] must be left alone; on older
				 * chips data[0] bit7 enables in-band sleep and the
				 * baudrate sits at data[2].
				 */
				if (wcn3991) {
					tlv_nvm->data[1] = config->user_baud_rate;
				} else {
					tlv_nvm->data[0] |= 0x80;
					tlv_nvm->data[2] = config->user_baud_rate;
				}

				break;

			case EDL_TAG_ID_DEEP_SLEEP:
				/* Sleep enable mask
				 * enabling deep sleep feature on controller.
				 * New NVM format: SIBS flag is data[1] bit0.
				 */
				if (wcn3991)
					tlv_nvm->data[1] |= 0x01;
				else
					tlv_nvm->data[0] |= 0x01;

				break;
			}

			idx += (sizeof(u16) + sizeof(u16) + 8 + tag_len);
		}
		break;

	default:
		BT_ERR("Unknown TLV type %d", config->type);
		break;
	}
}

static int qca_tlv_send_segment(struct hci_dev *hdev, int seg_size,
				 const u8 *data, enum rome_tlv_dnld_mode mode)
{
	struct sk_buff *skb;
	struct edl_event_hdr *edl;
	struct tlv_seg_resp *tlv_resp;
	u8 cmd[MAX_SIZE_PER_TLV_SEGMENT + 2];
	int err = 0;

	cmd[0] = EDL_PATCH_TLV_REQ_CMD;
	cmd[1] = seg_size;
	memcpy(cmd + 2, data, seg_size);

	if (mode == ROME_SKIP_EVT_VSE_CC || mode == ROME_SKIP_EVT_VSE)
		return __hci_cmd_send(hdev, EDL_PATCH_CMD_OPCODE, seg_size + 2,
				      cmd);

	skb = __hci_cmd_sync_ev(hdev, EDL_PATCH_CMD_OPCODE, seg_size + 2, cmd,
				HCI_EV_VENDOR, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		bt_dev_err(hdev, "QCA Failed to send TLV segment (%d)", err);
		return err;
	}

	/* WCN399x parts may ack each segment with a command complete
	 * event instead of a vendor specific TLV download event: the
	 * response is then just cresp + command echo without a result
	 * byte. */
	if (skb->len != sizeof(*edl) + sizeof(*tlv_resp) &&
	    skb->len != sizeof(*edl)) {
		bt_dev_err(hdev, "QCA TLV response size mismatch");
		err = -EILSEQ;
		goto out;
	}

	edl = (struct edl_event_hdr *)(skb->data);
	if (!edl) {
		bt_dev_err(hdev, "TLV with no header");
		err = -EILSEQ;
		goto out;
	}

	if (edl->cresp != EDL_CMD_REQ_RES_EVT) {
		bt_dev_err(hdev, "QCA TLV with error stat 0x%x rtype 0x%x",
			   edl->cresp, edl->rtype);
		err = -EIO;
		goto out;
	}

	if (edl->rtype == EDL_PATCH_TLV_REQ_CMD) {
		if (skb->len != sizeof(*edl)) {
			err = -EILSEQ;
			goto out;
		}
	} else if (edl->rtype == EDL_TVL_DNLD_RES_EVT) {
		if (skb->len != sizeof(*edl) + sizeof(*tlv_resp)) {
			err = -EILSEQ;
			goto out;
		}
		tlv_resp = (struct tlv_seg_resp *)(edl->data);
		if (tlv_resp->result != 0x00) {
			bt_dev_err(hdev, "QCA TLV with error stat 0x%x rtype 0x%x (0x%x)",
				   edl->cresp, edl->rtype, tlv_resp->result);
			err = -EIO;
		}
	} else {
		bt_dev_err(hdev, "QCA TLV with error stat 0x%x rtype 0x%x",
			   edl->cresp, edl->rtype);
		err = -EIO;
	}

out:
	kfree_skb(skb);

	return err;
}

static int qca_download_firmware(struct hci_dev *hdev,
				  struct rome_config *config, bool wcn3991)
{
	const struct firmware *fw;
	const u8 *segment;
	u8 *data;
	int ret, remain, i = 0;

	bt_dev_info(hdev, "QCA Downloading %s", config->fwname);

	ret = request_firmware(&fw, config->fwname, &hdev->dev);
	if (ret) {
		bt_dev_err(hdev, "QCA Failed to request file: %s (%d)",
			   config->fwname, ret);
		return ret;
	}

	/* qca_tlv_check_data() patches the NVM buffer, so work on a
	 * writable copy instead of the const firmware image. */
	data = kmemdup(fw->data, fw->size, GFP_KERNEL);
	remain = fw->size;
	release_firmware(fw);
	if (!data)
		return -ENOMEM;

	qca_tlv_check_data(config, data, wcn3991);

	segment = data;
	while (remain > 0) {
		int segsize = min(MAX_SIZE_PER_TLV_SEGMENT, remain);

		bt_dev_dbg(hdev, "Send segment %d, size %d", i++, segsize);

		remain -= segsize;
		/* The last segment is always acked regardless download mode */
		if (!remain || segsize < MAX_SIZE_PER_TLV_SEGMENT)
			config->dnld_mode = ROME_SKIP_EVT_NONE;

		ret = qca_tlv_send_segment(hdev, segsize, segment,
					    config->dnld_mode);
		if (ret)
			break;

		segment += segsize;
	}

	kfree(data);

	return ret;
}

int qca_set_bdaddr_rome(struct hci_dev *hdev, const bdaddr_t *bdaddr)
{
	struct sk_buff *skb;
	u8 cmd[9];
	int err;

	cmd[0] = EDL_NVM_ACCESS_SET_REQ_CMD;
	cmd[1] = 0x02; 			/* TAG ID */
	cmd[2] = sizeof(bdaddr_t);	/* size */
	memcpy(cmd + 3, bdaddr, sizeof(bdaddr_t));
	skb = __hci_cmd_sync_ev(hdev, EDL_NVM_ACCESS_OPCODE, sizeof(cmd), cmd,
				HCI_EV_VENDOR, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		bt_dev_err(hdev, "QCA Change address command failed (%d)", err);
		return err;
	}

	kfree_skb(skb);

	return 0;
}
EXPORT_SYMBOL_GPL(qca_set_bdaddr_rome);

int qca_uart_setup(struct hci_dev *hdev, uint8_t baudrate,
		   enum qca_btsoc_type soc_type, u32 soc_ver, u32 soc_id)
{
	struct rome_config config;
	bool wcn3991;
	int err;
	u8 rom_ver;

	bt_dev_dbg(hdev, "QCA setup on UART");

	/* WCN3991 (Cherokee) parts report the same soc_type as WCN3990
	 * but answer EDL commands with command complete events, select
	 * the 'u' NVM variant and need SoC logging disabled before the
	 * controller reboots into the downloaded firmware. Identify
	 * them by their SoC ID.
	 */
	wcn3991 = soc_id == QCA_WCN3991_SOC_ID;

	config.user_baud_rate = baudrate;

	/* Download rampatch file */
	config.type = TLV_TYPE_PATCH;
	if (soc_type == QCA_WCN3990) {
		/* Firmware files to download are based on ROM version.
		 * ROM version is derived from last two bytes of soc_ver.
		 */
		rom_ver = ((soc_ver & 0x00000f00) >> 0x04) |
			    (soc_ver & 0x0000000f);
		snprintf(config.fwname, sizeof(config.fwname),
			 "qca/crbtfw%02x.tlv", rom_ver);
	} else {
		snprintf(config.fwname, sizeof(config.fwname),
			 "qca/rampatch_%08x.bin", soc_ver);
	}

	err = qca_download_firmware(hdev, &config, wcn3991);
	if (err < 0) {
		bt_dev_err(hdev, "QCA Failed to download patch (%d)", err);
		return err;
	}

	/* Give the controller some time to get ready to receive the NVM */
	msleep(10);

	/* Download NVM configuration */
	config.type = TLV_TYPE_NVM;
	if (soc_type == QCA_WCN3990)
		snprintf(config.fwname, sizeof(config.fwname),
			 wcn3991 ? "qca/crnv%02xu.bin" : "qca/crnv%02x.bin",
			 rom_ver);
	else
		snprintf(config.fwname, sizeof(config.fwname),
			 "qca/nvm_%08x.bin", soc_ver);

	err = qca_download_firmware(hdev, &config, wcn3991);
	if (err < 0) {
		bt_dev_err(hdev, "QCA Failed to download NVM (%d)", err);
		return err;
	}

	/* The controller now reboots into the downloaded firmware. The
	 * caller enables in-band sleep handling and performs the HCI
	 * reset — the NVM config arms controller-side IBS, so commands
	 * sent before the host state machine engages would be lost. */
	bt_dev_info(hdev, "QCA firmware download completed");

	return 0;
}
EXPORT_SYMBOL_GPL(qca_uart_setup);

MODULE_AUTHOR("Ben Young Tae Kim <ytkim@qca.qualcomm.com>");
MODULE_DESCRIPTION("Bluetooth support for Qualcomm Atheros family ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
