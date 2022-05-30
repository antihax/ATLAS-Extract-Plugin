const {parentPort, threadId} = require("worker_threads");
const execSync = require("child_process").execSync;

parentPort.on("message", (task) => {
	const {cmd, grid} = task;
	console.log(`running ${grid} on worker ${threadId}`);
	parentPort.postMessage(execSync(cmd));
});
