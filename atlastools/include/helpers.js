const fs = require("fs");
const config = {
	"GPSBounds": {
		"max": [
			100,
			-100
		],
		"min": [
			-100,
			100
		]
	},
	"GridOffset": 23333.333333333332,
	"GridSize": 1400000,
	"NodesPerAxis": 30,
	"ServersX": 11,
	"ServersY": 11,
	"XRange": 200,
	"XScale": 1,
	"YRange": 200,
	"YScale": 1,
	"AtlasMapServer": false,
	"ItemLink": true,
	"KofiLink": true,
	"WarehouseTool": true,
	"PathFinder": true,
	"PinTool": true
};
module.exports = {
	sleep: function (ms) {
		return new Promise((resolve) => setTimeout(resolve, ms));
	},

	stop: async function () {
		await this.sleep(200000000);
	},

	contains: function (list, search) {
		for (let i in search) {
			for (let x in list) {
				if (x == search[i]) {
					return true;
				}
			}
		}
		return false;
	},

	includes: function (list, search) {
		for (let x in list) {
			let i = list[x];
			if (i === search) {
				return true;
			}
		}
		return false;
	},

	hasType: function (list, type) {
		for (let x in list) {
			if (x.startsWith(type)) {
				return true;
			}
		}
		return false;
	},

	gridName: function (x, y) {
		return String.fromCharCode(65 + x) + (1 + y);
	},

	constraint: function (value, minVal, maxVal, minRange, maxRange) {
		return ((value - minVal) / (maxVal - minVal)) * (maxRange - minRange);
	},

	parseJSONFile: function (file) {
		const rawdata = fs.readFileSync(file);
		return JSON.parse(rawdata);
	},

	GPStoLeaflet: function (x, y) {
		let long = (y - config.GPSBounds.min[1]) * config.XScale * 1.28,
			lat = (x - config.GPSBounds.min[0]) * config.YScale * 1.28;

		return [long, lat];
	},

	/* function GPStoLeaflet(x, y) {
        var long = ((y - 100) * 0.3636363636363636) * 1.28  ,
                  lat = ((100 + x) * 0.3636363636363636) * 1.28 ;
      
                  return [long, lat];
      },*/

	translateGPS: function (c) {
		let y = this.constraint(c[1], -400, 50, 0, 200) - 100,
			x = this.constraint(c[0], 100, 550, 0, 200) - 100;
		//  console.log(x, y)
		return [x, y];
	},

	sortObjByKey: function (value) {
		return typeof value === "object"
			? Array.isArray(value)
				? value.map(sortObjByKey)
				: Object.keys(value)
						.sort()
						.reduce((o, key) => {
							const v = value[key];
							o[key] = sortObjByKey(v);
							return o;
						}, {})
			: value;
	},

	clone: function (obj) {
		if (null == obj || "object" != typeof obj) return obj;
		var copy = obj.constructor();
		for (var attr in obj) {
			if (obj.hasOwnProperty(attr)) copy[attr] = obj[attr];
		}
		return copy;
	},

	inside: function (i, c) {
		let x1 = i.worldX - i.islandHeight / 2,
			y1 = i.worldY - i.islandWidth / 2,
			x2 = i.worldX + i.islandHeight / 2,
			y2 = i.worldY + i.islandWidth / 2,
			x = c[0],
			y = c[1];

		if (x > x1 && x < x2 && y > y1 && y < y2) {
			return true;
		}
		return false;
	},

	distance: function (c1, c2) {
		var xs = c1[0] - c2[0],
			ys = c1[1] - c2[1];

		xs *= xs;
		ys *= ys;

		return Math.sqrt(xs + ys);
	}
};
