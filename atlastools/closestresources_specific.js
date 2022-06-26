"use strict";
require("tty");
const fs = require("fs");
const {nextTick} = require("process");

let rawdata = fs.readFileSync("json/islands.json");
let islands = JSON.parse(rawdata);

rawdata = fs.readFileSync("resourceTypes.json");
let types = JSON.parse(rawdata);

var home = islands[1675]; // 1675 J9
var home2 = islands[424]; // 424  J10
var home3 = islands[1235]; // 1235 J7
var home4 = islands[181]; // 181  I10
var home5 = islands[347]; // 347
var home6 = islands[52]; // 52

home.types = {};
for (var island in islands) {
	var i = islands[island];
	if (i.name.includes("Trench") || i.name.includes("PVE") || i.name.includes("Cave")) continue;

	var dist = distance(home.worldX, home.worldY, i.worldX, i.worldY);
	var dist2 = distance(home2.worldX, home2.worldY, i.worldX, i.worldY);
	var dist3 = distance(home3.worldX, home3.worldY, i.worldX, i.worldY);
	var dist4 = distance(home4.worldX, home4.worldY, i.worldX, i.worldY);
	var dist5 = distance(home5.worldX, home5.worldY, i.worldX, i.worldY);
	var dist6 = distance(home6.worldX, home6.worldY, i.worldX, i.worldY);

	for (var type in types) {
		if (
			hasType(i.resources, type) &&
			types[type] != "Base" &&
			types[type] != "Berry" &&
			types[type] != "Crystal" &&
			types[type] != "Gem" &&
			types[type] != "Oil" &&
			types[type] != "Vegetable" &&
			types[type] != "Salt" &&
			types[type] != "Sugar" &&
			types[type] != "Ignore" &&
			types[type] != "Special" &&
			types[type] != "Coal" &&
			types[type] != "Seed" &&
			types[type] != "Herb" &&
			types[type] != "Herb"
		) {
			if (!home.types[type] || home.types[type].distance > dist) {
				home.types[type] = {
					distance: dist,
					grid: i.grid,
					id: i.id,
					type: types[type]
				};
			}
			if (!home.types[type] || home.types[type].distance > dist2) {
				home.types[type] = {
					distance: dist2,
					grid: i.grid,
					id: i.id,
					type: types[type]
				};
			}
			if (!home.types[type] || home.types[type].distance > dist3) {
				home.types[type] = {
					distance: dist3,
					grid: i.grid,
					id: i.id,
					type: types[type]
				};
			}
			if (!home.types[type] || home.types[type].distance > dist4) {
				home.types[type] = {
					distance: dist4,
					grid: i.grid,
					id: i.id,
					type: types[type]
				};
			}
			if (!home.types[type] || home.types[type].distance > dist5) {
				home.types[type] = {
					distance: dist5,
					grid: i.grid,
					id: i.id,
					type: types[type]
				};
			}
			if (!home.types[type] || home.types[type].distance > dist6) {
				home.types[type] = {
					distance: dist6,
					grid: i.grid,
					id: i.id,
					type: types[type]
				};
			}
		}
	}
}

for (let t in home.types) {
	if (home.types[t].distance > 0) console.log(t, home.types[t]);
}

//console.log(home.types);

// distance in toroidal space
function distance(x1, y1, x2, y2) {
	var dx = Math.abs(x2 - x1);
	var dy = Math.abs(y2 - y1);

	if (dx > 7700000) dx = 15400000 - dx;

	if (dy > 7700000) dy = 15400000 - dy;

	return Math.sqrt(dx * dx + dy * dy);
}

function sleep(ms) {
	return new Promise((resolve) => setTimeout(resolve, ms));
}

async function demo() {
	await sleep(20000);

	// Sleep in loop
	for (let i = 0; i < 50; i++) {
		if (i === 3) await sleep(2000);
	}
}

demo();

function contains(list, search) {
	for (var i in search) {
		for (var x in list) {
			if (x == search[i]) {
				return true;
			}
		}
	}
	return false;
}

function isA(type, search) {
	for (var x in search) {
		if (x === type) {
			return true;
		}
	}
	return false;
}

function hasType(list, type) {
	for (var x in list) {
		if (x.startsWith(type)) {
			return true;
		}
	}
	return false;
}
