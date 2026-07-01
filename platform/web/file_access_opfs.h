#pragma once

#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"

class FileAccessOPFS : public FileAccess {
	GDSOFTCLASS(FileAccessOPFS, FileAccess);

	Vector<uint8_t> data;
	mutable uint64_t pos = 0;
	bool open_flag = false;

public:
	static void bridge_setup();
	static int bridge_read(const String &p_rel, Vector<uint8_t> &r_data);

	virtual Error open_internal(const String &p_path, int p_mode_flags) override;
	virtual bool is_open() const override { return open_flag; }

	virtual void seek(uint64_t p_position) override { pos = p_position; }
	virtual void seek_end(int64_t p_position) override { pos = data.size() + p_position; }
	virtual uint64_t get_position() const override { return pos; }
	virtual uint64_t get_length() const override { return data.size(); }

	virtual bool eof_reached() const override { return pos >= (uint64_t)data.size(); }

	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;

	virtual Error get_error() const override { return eof_reached() ? ERR_FILE_EOF : OK; }

	virtual Error resize(int64_t p_length) override { return ERR_UNAVAILABLE; }
	virtual void flush() override {}
	virtual bool store_buffer(const uint8_t *p_src, uint64_t p_length) override { return false; }

	virtual bool file_exists(const String &p_name) override;

	virtual uint64_t _get_modified_time(const String &p_file) override { return 0; }
	virtual uint64_t _get_access_time(const String &p_file) override { return 0; }
	virtual int64_t _get_size(const String &p_file) override { return -1; }

	virtual BitField<FileAccess::UnixPermissionFlags> _get_unix_permissions(const String &p_file) override { return 0; }
	virtual Error _set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) override { return FAILED; }

	virtual bool _get_hidden_attribute(const String &p_file) override { return false; }
	virtual Error _set_hidden_attribute(const String &p_file, bool p_hidden) override { return ERR_UNAVAILABLE; }
	virtual bool _get_read_only_attribute(const String &p_file) override { return false; }
	virtual Error _set_read_only_attribute(const String &p_file, bool p_ro) override { return ERR_UNAVAILABLE; }

	virtual void close() override { open_flag = false; }
};

class PackedSourceOPFS : public PackSource {
public:
	virtual bool try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) override;
	virtual Ref<FileAccess> get_file(const String &p_path, PackedData::PackedFile *p_file) override;
};

void register_web_pack_source();
