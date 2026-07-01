const GodotOPFS = {
	$GodotOPFS__deps: ['$GodotRuntime'],
	$GodotOPFS: {
		workers: [],
		WORKER_SRC: [
			'let memory, base;',
			'async function readFile(rel) {',
			'	const parts = ("godot-res/" + rel).split("/").filter(Boolean);',
			'	const root = await navigator.storage.getDirectory();',
			'	let dir = root;',
			'	for (let i = 0; i < parts.length - 1; i++) { dir = await dir.getDirectoryHandle(parts[i]); }',
			'	const fh = await dir.getFileHandle(parts[parts.length - 1]);',
			'	return new Uint8Array(await (await fh.getFile()).arrayBuffer());',
			'}',
			'async function loop() {',
			'	for (;;) {',
			'		const iw = new Int32Array(memory.buffer);',
			'		const w = Atomics.waitAsync(iw, base, 0);',
			'		if (w.async) { await w.value; }',
			'		Atomics.store(iw, base, 0);',
			'		const pathPtr = iw[base + 2], pathLen = iw[base + 3], destPtr = iw[base + 4], destCap = iw[base + 5];',
			'		const pb = new Uint8Array(pathLen);',
			'		pb.set(new Uint8Array(memory.buffer).subarray(pathPtr, pathPtr + pathLen));',
			'		const rel = new TextDecoder().decode(pb);',
			'		let resLen = -1;',
			'		try {',
			'			const bytes = await readFile(rel);',
			'			new Uint8Array(memory.buffer).set(bytes.subarray(0, Math.min(bytes.length, destCap)), destPtr);',
			'			resLen = bytes.length;',
			'		} catch (e) { resLen = -1; }',
			'		const ir = new Int32Array(memory.buffer);',
			'		ir[base + 6] = resLen;',
			'		Atomics.store(ir, base + 1, 1);',
			'		Atomics.notify(ir, base + 1);',
			'	}',
			'}',
			'self.onmessage = function (e) {',
			'	memory = e.data.memory;',
			'	base = e.data.ctrlPtr >> 2;',
			'	loop();',
			'};',
		].join('\n'),
		setup: function (ctrlPtr) {
			const url = URL.createObjectURL(new Blob([GodotOPFS.WORKER_SRC], { type: 'application/javascript' }));
			const worker = new Worker(url);
			worker.postMessage({ memory: wasmMemory, ctrlPtr: ctrlPtr });
			GodotOPFS.workers.push(worker);
		},
	},

	godot_js_opfs_setup__proxy: 'sync',
	godot_js_opfs_setup__sig: 'vi',
	godot_js_opfs_setup: function (p_ctrl_ptr) {
		GodotOPFS.setup(p_ctrl_ptr);
	},

	godot_js_opfs_read__sig: 'iiiiii',
	godot_js_opfs_read: function (p_ctrl_ptr, p_path_ptr, p_path_len, p_dest_ptr, p_dest_cap) {
		const c = p_ctrl_ptr >> 2;
		HEAP32[c + 2] = p_path_ptr;
		HEAP32[c + 3] = p_path_len;
		HEAP32[c + 4] = p_dest_ptr;
		HEAP32[c + 5] = p_dest_cap;
		Atomics.store(HEAP32, c + 1, 0);
		Atomics.store(HEAP32, c, 1);
		Atomics.notify(HEAP32, c);
		Atomics.wait(HEAP32, c + 1, 0);
		return HEAP32[c + 6];
	},
};

autoAddDeps(GodotOPFS, '$GodotOPFS');
mergeInto(LibraryManager.library, GodotOPFS);
