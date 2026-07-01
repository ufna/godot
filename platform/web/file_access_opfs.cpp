#include "file_access_opfs.h"

#include "core/os/mutex.h"
#include "core/os/semaphore.h"

extern "C" {
extern void godot_js_opfs_setup(int32_t *p_ctrl);
extern int godot_js_opfs_read(int32_t *p_ctrl, const char *p_path, int p_path_len, uint8_t *p_dest, int p_dest_cap);
}

static const int OPFS_POOL = 8;
static int32_t opfs_ctrl[OPFS_POOL][8] = {};
static Semaphore opfs_sem;
static Mutex opfs_free_mutex;
static Vector<int> opfs_free;
static bool opfs_ready = false;

void FileAccessOPFS::bridge_setup() {
	if (opfs_ready) {
		return;
	}
	for (int i = 0; i < OPFS_POOL; i++) {
		godot_js_opfs_setup(opfs_ctrl[i]);
		opfs_free.push_back(i);
		opfs_sem.post();
	}
	opfs_ready = true;
}

int FileAccessOPFS::bridge_read(const String &p_rel, Vector<uint8_t> &r_data) {
	CharString cs = p_rel.utf8();
	if (r_data.size() == 0) {
		r_data.resize(65536);
	}
	opfs_sem.wait();
	opfs_free_mutex.lock();
	const int slot = opfs_free[opfs_free.size() - 1];
	opfs_free.remove_at(opfs_free.size() - 1);
	opfs_free_mutex.unlock();

	int32_t *ctrl = opfs_ctrl[slot];
	int n = godot_js_opfs_read(ctrl, cs.get_data(), cs.length(), r_data.ptrw(), r_data.size());
	if (n >= 0 && n > (int)r_data.size()) {
		r_data.resize(n);
		n = godot_js_opfs_read(ctrl, cs.get_data(), cs.length(), r_data.ptrw(), r_data.size());
	}

	opfs_free_mutex.lock();
	opfs_free.push_back(slot);
	opfs_free_mutex.unlock();
	opfs_sem.post();

	if (n < 0) {
		return -1;
	}
	r_data.resize(n);
	return n;
}

Error FileAccessOPFS::open_internal(const String &p_path, int p_mode_flags) {
	if (p_mode_flags & FileAccess::WRITE) {
		return ERR_UNAVAILABLE;
	}
	const String rel = p_path.trim_prefix("res://");
	data.clear();
	const int n = bridge_read(rel, data);
	if (n < 0) {
		open_flag = false;
		return ERR_FILE_CANT_OPEN;
	}
	pos = 0;
	open_flag = true;
	return OK;
}

uint64_t FileAccessOPFS::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	const uint64_t left = (uint64_t)data.size() - pos;
	const uint64_t to_read = MIN(p_length, left);
	if (to_read > 0) {
		memcpy(p_dst, data.ptr() + pos, to_read);
		pos += to_read;
	}
	return to_read;
}

bool FileAccessOPFS::file_exists(const String &p_name) {
	const String rel = p_name.trim_prefix("res://");
	Vector<uint8_t> tmp;
	return bridge_read(rel, tmp) >= 0;
}

static uint32_t opfs_rd_u32(const uint8_t *p, uint64_t &off) {
	const uint32_t v = (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) | ((uint32_t)p[off + 2] << 16) | ((uint32_t)p[off + 3] << 24);
	off += 4;
	return v;
}

bool PackedSourceOPFS::try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) {
	if (!p_path.begins_with("opfs://")) {
		return false;
	}
	Vector<uint8_t> idx;
	if (FileAccessOPFS::bridge_read(".godot_opfs_index", idx) < 0) {
		return false;
	}
	const uint8_t *p = idx.ptr();
	const uint64_t total = idx.size();
	if (total < 4) {
		return false;
	}
	uint64_t off = 0;
	const uint32_t count = opfs_rd_u32(p, off);
	uint8_t md5[16] = { 0 };
	for (uint32_t i = 0; i < count; i++) {
		if (off + 4 > total) {
			break;
		}
		const uint32_t sl = opfs_rd_u32(p, off);
		if (off + (uint64_t)sl + 4 > total) {
			break;
		}
		const String path = String::utf8((const char *)(p + off), sl);
		off += sl;
		const uint32_t size = opfs_rd_u32(p, off);
		PackedData::get_singleton()->add_path(p_path, "res://" + path, 0, size, md5, this, p_replace_files, false, false, false);
	}
	return true;
}

Ref<FileAccess> PackedSourceOPFS::get_file(const String &p_path, PackedData::PackedFile *p_file) {
	Ref<FileAccessOPFS> f;
	f.instantiate();
	if (f->open_internal(p_path, FileAccess::READ) != OK) {
		return Ref<FileAccess>();
	}
	return f;
}

void register_web_pack_source() {
	FileAccessOPFS::bridge_setup();
	PackedData::get_singleton()->add_pack_source(memnew(PackedSourceOPFS));
}
