const {AsyncResource} = require("async_hooks");
const {EventEmitter} = require("events");
const path = require("path");
const {Worker} = require("worker_threads");

const kTaskInfo = Symbol("kTaskInfo");
const kWorkerFreedEvent = Symbol("kWorkerFreedEvent");

class WorkerPoolTaskInfo extends AsyncResource {
	constructor(callback) {
		super("WorkerPoolTaskInfo");
		this.callback = callback;
	}

	done(err, result) {
		this.runInAsyncScope(this.callback, null, err, result);
		this.emitDestroy(); // `TaskInfo`s are used only once.
	}
}

class WorkerPool extends EventEmitter {
	constructor(numThreads, workerFile) {
		super();
		this.numThreads = numThreads;
		this.workerFile = workerFile;
		this.workers = [];
		this.freeWorkers = [];

		for (let i = 0; i < numThreads; i++) this.addNewWorker();
	}

	addNewWorker() {
		const worker = new Worker(path.resolve(this.workerFile));
		worker.on("message", (result) => {
			// In case of success: Call the callback that was passed to `runTask`,
			// remove the `TaskInfo` associated with the Worker, and mark it as free
			// again.
			worker[kTaskInfo].done(null, result);
			worker[kTaskInfo] = null;
			this.freeWorkers.push(worker);
			this.emit(kWorkerFreedEvent);
		});
		worker.on("error", (err) => {
			// In case of an uncaught exception: Call the callback that was passed to
			// `runTask` with the error.
			if (worker[kTaskInfo]) worker[kTaskInfo].done(err, null);
			else this.emit("error", err);
			// Remove the worker from the list and start a new Worker to replace the
			// current one.
			this.workers.splice(this.workers.indexOf(worker), 1);
			this.addNewWorker();
		});
		this.workers.push(worker);
		this.freeWorkers.push(worker);
		this.emit(kWorkerFreedEvent);
	}

	runTask(task, callback) {
		if (this.freeWorkers.length === 0) {
			// No free threads, wait until a worker thread becomes free.
			this.once(kWorkerFreedEvent, () => this.runTask(task, callback));
			return;
		}

		const worker = this.freeWorkers.pop();
		worker[kTaskInfo] = new WorkerPoolTaskInfo(callback);
		worker.postMessage(task);
	}

	close() {
		for (const worker of this.workers) worker.terminate();
	}
}

module.exports = WorkerPool;
