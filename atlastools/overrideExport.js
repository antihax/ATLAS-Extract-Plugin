// Check for broken or missing nodes

"use strict";
const helpers = require("./include/helpers.js");

const fs = require("fs");

let data = JSON.parse(fs.readFileSync("overrides.json"));

data.forEach((e) => {
	fs.open(`csv/${e.Key}.csv`, "a", 0o644, (err, fd) => {
		Object.keys(e.FoliageMap).forEach((k) => {
			fs.writeSync(fd, `${k},${e.FoliageMap[k]}\n`, null, undefined);
		});
	});
});
