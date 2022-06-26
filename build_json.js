const fs = require("fs");
const WorkerPool = require("./atlastools/include/workerpool.js");
const execSync = require("child_process").execSync;
const helpers = require("./atlastools/include/helpers.js");
const path = require("./atlastools/include/ngraph.path");
const graph = require("./atlastools/include/ngraph.graph");
const baseDir = "DumpResources/server/ShooterGame/";
const resourceDir = baseDir + "Binaries/Win64/resources/";
const workDir = process.cwd();
const os = require("os");
const serverConfig = helpers.parseJSONFile(baseDir + "ServerGrid.json");
const xGrids = serverConfig.totalGridsX;
const yGrids = serverConfig.totalGridsY;
const worldUnitsX = xGrids * serverConfig.gridSize;
const worldUnitsY = yGrids * serverConfig.gridSize;

async function buildData() {
	const pool = new WorkerPool(os.cpus().length / 3, "./build_worker.js");
	pool.setMaxListeners(250);
	let extraArgs = "";
	if (process.argv.includes("manualmanagedmods")) {
		extraArgs += " -manualmanagedmods";
	}
	process.chdir("DumpResources");

	let tasks = [];

	for (let y = 0; y < yGrids; y++) {
		for (let x = 0; x < xGrids; x++) {
			let cmd =
				"start /min " +
				process.cwd() +
				`\\server\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe Ocean?ServerX=${x}?ServerY=${y}?AltSaveDirectoryName=${x}${y}?ServerAdminPassword=123?QueryPort=5${x}${y}?Port=6${x}${y}?MapPlayerLocation=true -NoBattlEye -log -server -NoSeamlessServer ${extraArgs}`;
			let grid = helpers.gridName(x, y);
			let task = new Promise((resolve) => {
				pool.runTask({cmd, grid}, (err, result) => {
					return resolve(result);
				});
			});
			tasks.push(task);
		}
	}
	await Promise.all(tasks);
	process.chdir(workDir);
}

async function main() {
	if (!process.argv.includes("nobuild")) {
		await buildData();
	}
	console.log("processing...");
	// Process stuff
	const gpsBounds = helpers.parseJSONFile(resourceDir + "gpsbounds.json");

	// load resources
	let grids = {};
	let stones = [];
	let bosses = [];
	let altars = [];
	for (let x = 0; x < xGrids; x++) {
		for (let y = 0; y < yGrids; y++) {
			let grid = helpers.gridName(x, y);
			grids[grid] = helpers.parseJSONFile(resourceDir + grid + ".json");

			// save power stones
			if (grids[grid].Stones)
				for (let stone in grids[grid].Stones)
					stones.push({
						name: stone,
						long: grids[grid].Stones[stone][0],
						lat: grids[grid].Stones[stone][1]
					});

			// save bosses
			if (grids[grid].Boss)
				for (let boss in grids[grid].Boss)
					for (let position in grids[grid].Boss[boss])
						bosses.push({
							name: boss,
							long: grids[grid].Boss[boss][position][0],
							lat: grids[grid].Boss[boss][position][1]
						});
			// save altars
			if (grids[grid].Altar)
				for (let altar in grids[grid].Altar)
					for (let position in grids[grid].Altar[altar])
						altars.push({
							name: altar,
							long: grids[grid].Altar[altar][position][0],
							lat: grids[grid].Altar[altar][position][1]
						});
		}
	}
	let animalcheck = new Set();
	let allassets = new Set();
	let allassettype = {};
	// match data to island instances
	let islands = {};
	let islandExtended = {};
	let gridList = {};
	let regions = {};

	for (let server in serverConfig.servers) {
		let s = serverConfig.servers[server];

		let grid = helpers.gridName(s.gridX, s.gridY);
		let gridBiomes = new Set();
		let gridAnimals = new Set();
		let gridResources = new Set();

		gridList[grid] = {};
		gridList[grid].name = s.name;
		if (s.hiddenAtlasId === "") {
			s.hiddenAtlasId = serverConfig.MainRegionName;
		}
		gridList[grid].region = s.hiddenAtlasId;
		gridList[grid].serverCustomDatas1 = s.ServerCustomDatas1;
		gridList[grid].serverCustomDatas2 = s.ServerCustomDatas2;
		gridList[grid].serverTemplate = s.serverTemplateName;
		gridList[grid].serverIslandPointsMultiplier = s.serverIslandPointsMultiplier;
		gridList[grid].forceServerRules = s.forceServerRules;
		gridList[grid].forceServerRules = s.forceServerRules;

		gridList[grid].DestNorth = [s.OverrideDestNorthX, s.OverrideDestNorthY];
		gridList[grid].DestSouth = [s.OverrideDestSouthX, s.OverrideDestSouthY];
		gridList[grid].DestEast = [s.OverrideDestEastX, s.OverrideDestEastY];
		gridList[grid].DestWest = [s.OverrideDestWestX, s.OverrideDestWestY];

		if (typeof regions[s.hiddenAtlasId] === "undefined") {
			regions[s.hiddenAtlasId] = {
				MinX: s.gridX,
				MinY: s.gridY,
				MaxX: s.gridX,
				MaxY: s.gridY
			};
		} else {
			let r = regions[s.hiddenAtlasId];

			// Grow region bounds elastically
			if (r.MaxX < s.gridX && r.MaxX + 1 === s.gridX) {
				r.MaxX = s.gridX;
				r.MaxY = s.gridY; // hack to fix over extending
			}
			// Grow as a rectangle
			if (r.MaxY < s.gridY && r.MaxY + 1 === s.gridY) {
				r.MaxY = s.gridY;
			}
		}

		gridList[grid].biomes = new Set();

		for (let island in s.islandInstances) {
			let cp = s.islandInstances[island];
			let i = {
				id: cp.id,
				worldX: cp.worldX,
				worldY: cp.worldY,
				rotation: cp.rotation,
				name: cp.name,
				region: s.hiddenAtlasId,
				islandWidth: cp.islandWidth,
				islandHeight: cp.islandHeight,
				isControlPoint: cp.isControlPoint
			};
			i.sublevels = [];
			for (let sl in s.sublevels) {
				let sublevel = s.sublevels[sl];
				if (sublevel.id === i.id) {
					i.sublevels.push(sublevel.name);
				}
			}
			islands[i.id] = i;

			i.grid = grid;
			i.homeServer = s.isHomeServer ? true : false;
			i.resources = {};
			i.discoveries = [];

			if (s.serverTemplateName) gridBiomes.add(s.serverTemplateName);

			if (i.treasureMapSpawnPoints && i.treasureMapSpawnPoints.length > 0) i.resources["Treasure Spawns"] = i.treasureMapSpawnPoints.length;

			// build resource map
			for (let r in grids[grid].Resources) {
				if (helpers.inside(i, GPSToWorld(Number(r.split(":")[0]), Number(r.split(":")[1]), gpsBounds)))
					for (let key in grids[grid].Resources[r]) {
						gridResources.add(key);
						if (!i.resources[key]) i.resources[key] = grids[grid].Resources[r][key];
						else i.resources[key] += grids[grid].Resources[r][key];
					}
			}

			// build animal list
			let allanimals = new Set();
			for (let r in grids[grid].Animals) {
				if (helpers.inside(i, GPSToWorld(Number(r.split(":")[0]), Number(r.split(":")[1]), gpsBounds)))
					for (let key in grids[grid].Animals[r]) {
						let animalList = grids[grid].Animals[r][key];
						for (let a in animalList.animals) {
							let animal = animalList.animals[a];
							gridAnimals.add(animal.name);
							allanimals.add(animal.name);
							animalcheck.add(animal.name);

							// print out special animals, check for changes

							let test = {
								"animals": [{"name": "Parrot"}, {"name": "Parrot"}, {"name": "Parrot"}],
								"gps": [[-9.99740219116211, -29.757213592529297]],
								"islandLevelMultiplier": 1.0,
								"islandLevelOffset": 0,
								"levelLerp": 0.0,
								"levelMinOveride": 0,
								"levelMultiplier": 1.0,
								"levelOffset": 0,
								"spawnLimits": [5.0, 80, 45.0]
							};
							if (animal.name === "Tiger" && animalList.levelOffset < 60 && animalList.spawnLimits[0] > 24) console.log(i.id, grid, animalList.spawnLimits, animal);
						}
					}
			}
			i.animals = Array.from(allanimals).sort();

			for (let r in grids[grid].Maps) {
				if (helpers.inside(i, GPSToWorld(Number(r.split(":")[0]), Number(r.split(":")[1]), gpsBounds))) i.maps = grids[grid].Maps[r];
			}

			for (let r in s.discoZones) {
				let disco = s.discoZones[r];

				if (disco.ManualVolumeName) {
					if (grids[grid].Discoveries) {
						let d = grids[grid].Discoveries[disco.ManualVolumeName];
						if (d) {
							i.discoveries.push({
								name: disco.name,
								long: d[0],
								lat: d[1]
							});
						}
					}
				} else {
					if (helpers.inside(i, [disco.worldX, disco.worldY])) {
						let coords = worldToGPS(disco.worldX, disco.worldY, gpsBounds);
						i.discoveries.push({
							name: disco.name,
							long: coords[0],
							lat: coords[1]
						});
					}
				}
			}

			let biomes = new Set();
			let biomeTags = new Set();
			for (let r in grids[grid].Biomes) {
				if (helpers.inside(i, GPSToWorld(Number(r.split(":")[0]), Number(r.split(":")[1]), gpsBounds))) {
					biomes.add(grids[grid].Biomes[r]);
					for (let b in grids[grid].Biomes[r].tags) {
						biomeTags.add(grids[grid].Biomes[r].tags[b]);
					}
				}
				gridBiomes.add(grids[grid].Biomes[r].name);
			}

			i.biomes = Array.from(biomes).sort();
			i.biomeTags = Array.from(biomeTags).sort();

			islandExtended[i.id] = helpers.clone(i);
			i = islandExtended[i.id];
			for (let r in grids[grid].Meshes) {
				if (helpers.inside(i, GPSToWorld(Number(r.split(":")[0]), Number(r.split(":")[1]), gpsBounds))) i.meshes = grids[grid].Meshes[r];
			}
			// build resource map
			i.assets = {};
			for (let r in grids[grid].Assets) {
				if (helpers.inside(i, GPSToWorld(Number(r.split(":")[0]), Number(r.split(":")[1]), gpsBounds))) {
					for (let key in grids[grid].Assets[r]) {
						if (!i.assets[key]) {
							allassets.add(key);
							i.assets[key] = grids[grid].Assets[r][key];
						} else {
							i.assets[key] += grids[grid].Assets[r][key];
						}
					}
				}
			}
			for (let asset in grids[grid].AssetType) {
				if (!allassettype[asset]) allassettype[asset] = {};
				for (let r in grids[grid].AssetType[asset]) {
					if (!allassettype[asset][r]) allassettype[asset][r] = 0;
					allassettype[asset][r] += grids[grid].AssetType[asset][r];
				}
			}
		}

		gridList[grid].biomes = Array.from(gridBiomes);
		gridList[grid].animals = Array.from(gridAnimals).sort();
		gridList[grid].resources = Array.from(gridResources).sort();
		gridList[grid].sotd = grids[grid].SoTD;
		gridList[grid].flotsam = grids[grid].Flotsam;
		gridList[grid].sunktreasure = grids[grid].SunkenTreasure;
	}

	console.log("completed build...");
	const nodesPerAxis = 30;
	let gridSize = serverConfig.gridSize;
	let gridOffset = gridSize / nodesPerAxis / 2;

	// save everything
	fs.writeFileSync(
		"./json/config.js",
		"const config = " +
			JSON.stringify(
				sortObjByKey({
					ServersX: serverConfig.totalGridsX,
					ServersY: serverConfig.totalGridsY,
					GridSize: serverConfig.gridSize,
					GPSBounds: gpsBounds,
					XRange: Math.abs(gpsBounds.min[0] - gpsBounds.max[0]),
					YRange: Math.abs(gpsBounds.min[1] - gpsBounds.max[1]),
					XScale: 200 / Math.abs(gpsBounds.min[0] - gpsBounds.max[0]),
					YScale: 200 / Math.abs(gpsBounds.min[1] - gpsBounds.max[1]),
					NodesPerAxis: nodesPerAxis,
					GridOffset: gridOffset
				}),
				null,
				"\t"
			)
	);

	fs.copyFileSync(resourceDir + "animals.json", "./json/animals.json");
	fs.copyFileSync(resourceDir + "items.json", "./json/items.json");
	fs.copyFileSync(resourceDir + "loottable.json", "./json/loottable.json");
	fs.copyFileSync(resourceDir + "structures.json", "./json/structures.json");
	fs.writeFileSync("./json/assets.json", JSON.stringify(Array.from(allassets).sort(), null, "\t"));
	fs.writeFileSync("./json/assettypes.json", JSON.stringify(sortObjByKey(allassettype), null, "\t"));
	fs.writeFileSync("./json/stones.json", JSON.stringify(sortObjByKey(stones), null, "\t"));
	fs.writeFileSync("./json/altars.json", JSON.stringify(sortObjByKey(altars), null, "\t"));
	fs.writeFileSync("./json/regions.json", JSON.stringify(sortObjByKey(regions), null, "\t"));
	fs.writeFileSync("./json/bosses.json", JSON.stringify(sortObjByKey(bosses), null, "\t"));
	fs.writeFileSync("./json/islands.json", JSON.stringify(sortObjByKey(islands), null, "\t"));
	fs.writeFileSync("./json/islandExtended.json", JSON.stringify(sortObjByKey(islandExtended), null, "\t"));
	fs.writeFileSync("./json/gridList.json", JSON.stringify(sortObjByKey(gridList), null, "\t"));
	fs.writeFileSync("./json/shipPaths.json", JSON.stringify(sortObjByKey(serverConfig.shipPaths), null, "\t"));
	fs.writeFileSync("./json/tradeWinds.json", JSON.stringify(sortObjByKey(serverConfig.tradeWinds), null, "\t"));
	fs.writeFileSync("./json/portals.json", JSON.stringify(sortObjByKey(serverConfig.portalPaths), null, "\t"));

	let g = graph();
	// Pass 1: build nodes and link
	for (let server in serverConfig.servers) {
		let s = serverConfig.servers[server];
		if (s.isMawWatersServer) continue; // skip maw waters server
		// loop west to east
		for (let x = s.gridX * gridSize + gridOffset; x < (s.gridX + 1) * gridSize; x += gridSize / nodesPerAxis) {
			let lastY;
			for (let y = s.gridY * gridSize + gridOffset; y < (s.gridY + 1) * gridSize; y += gridSize / nodesPerAxis) {
				let point = [x, y];
				let skip = false;
				for (let island in s.islandInstances) {
					// skip if the node is inside an island
					if (helpers.inside(s.islandInstances[island], point)) {
						skip = true;
						lastY = undefined;
						break;
					}
				}
				if (skip) continue;
				g.addNode(worldToGPS(x, y, gpsBounds).toString(), {});
				if (lastY !== undefined) {
					g.addLink(worldToGPS(x, y, gpsBounds).toString(), worldToGPS(x, lastY, gpsBounds).toString());
					g.addLink(worldToGPS(x, lastY, gpsBounds).toString(), worldToGPS(x, y, gpsBounds).toString());
					if (x > s.gridX * gridSize + gridOffset * 10)
						if (y > s.gridY * gridSize + gridOffset * 10) if (y < (s.gridY + 1) * gridSize - gridOffset * 10) if (x < (s.gridX + 1) * gridSize - gridOffset * 10) linkNodesNearest([x, y], g);
				}
				lastY = y;
			}
		}

		// loop north to south and link remaining nodes
		for (let y = s.gridY * gridSize + gridOffset; y < (s.gridY + 1) * gridSize; y += gridSize / nodesPerAxis) {
			let lastX;
			for (let x = s.gridX * gridSize + gridOffset; x < (s.gridX + 1) * gridSize; x += gridSize / nodesPerAxis) {
				let point = [x, y];
				let skip = false;
				for (let island in s.islandInstances) {
					// skip if the node is inside an island
					if (helpers.inside(s.islandInstances[island], point)) {
						skip = true;
						lastX = undefined;
						break;
					}
				}
				if (skip) continue;
				if (lastX !== undefined) {
					g.addLink(worldToGPS(x, y, gpsBounds).toString(), worldToGPS(lastX, y, gpsBounds).toString());
					g.addLink(worldToGPS(lastX, y, gpsBounds).toString(), worldToGPS(x, y, gpsBounds).toString());
				}
				lastX = x;
			}
		}
	}

	// Pass 2: link grid borders
	const region = helpers.parseJSONFile("./json/regions.json");
	for (let server in serverConfig.servers) {
		let s = serverConfig.servers[server];

		if (s.isMawWatersServer) continue; // skip maw waters server
		let bounds = getRegionBounds(s.gridX, s.gridY, region);
		if (bounds === undefined) continue; // skip if no region

		// North pass
		let y = s.gridY * gridSize + gridOffset;
		let count = 0;
		for (let x = s.gridX * gridSize + gridOffset; x < (s.gridX + 1) * gridSize; x += gridSize / nodesPerAxis) {
			let step = (gridSize / nodesPerAxis) * count++;
			let origin = worldToGPS(x, y, gpsBounds);
			// Override North transfer
			if (s.OverrideDestNorthX > -1) {
				let dX = s.OverrideDestNorthX * gridSize + gridOffset + step;
				let dY = (s.OverrideDestNorthY + 1) * gridSize - gridOffset;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
			// Wrap North Transfer, except single-grid regions
			else if (bounds.MinY === s.gridY && bounds.MinY != bounds.MaxY) {
				let dX = x;
				let dY = (bounds.MaxY + 1) * gridSize - gridOffset;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
			// North Transfer, except single-grid regions
			else if (bounds.MinY != bounds.MaxY) {
				let dX = x;
				let dY = s.gridY * gridSize - gridOffset;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
		}

		// South pass
		count = 0;
		y = (s.gridY + 1) * gridSize - gridOffset;
		for (let x = s.gridX * gridSize + gridOffset; x < (s.gridX + 1) * gridSize; x += gridSize / nodesPerAxis) {
			let step = (gridSize / nodesPerAxis) * count++;
			let origin = worldToGPS(x, y, gpsBounds);
			// Override South transfer
			if (s.OverrideDestSouthX > -1) {
				let dX = s.OverrideDestSouthX * gridSize + gridOffset + step;
				let dY = s.OverrideDestSouthY * gridSize + gridOffset;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
			// Wrap South Transfer, except single-grid regions
			else if (bounds.MaxY === s.gridY && bounds.MinY != bounds.MaxY) {
				let dX = x;
				let dY = bounds.MinY * gridSize + gridOffset;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
			// South Transfer, except single-grid regions
			else if (bounds.MinY != bounds.MaxY) {
				let dX = x;
				let dY = (s.gridY + 1) * gridSize + gridOffset;
				let destination = worldToGPS(dX, dY, gpsBounds);

				g.addLink(origin.toString(), destination.toString());
			}
		}

		// West pass
		count = 0;
		let x = s.gridX * gridSize + gridOffset;
		for (let y = s.gridY * gridSize + gridOffset; y < (s.gridY + 1) * gridSize; y += gridSize / nodesPerAxis) {
			let step = (gridSize / nodesPerAxis) * count++;
			let origin = worldToGPS(x, y, gpsBounds);
			// Override West transfer
			if (s.OverrideDestWestX > -1) {
				let dX = (s.OverrideDestWestX + 1) * gridSize - gridOffset;
				let dY = s.OverrideDestWestY * gridSize + gridOffset + step;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
			// Wrap West Transfer, except single-grid regions
			else if (bounds.MinX === s.gridX && bounds.MinX != bounds.MaxX) {
				let dX = (bounds.MaxX + 1) * gridSize - gridOffset;
				let dY = y;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
			// West Transfer, except single-grid regions
			else if (bounds.MinX != bounds.MaxX) {
				let dX = s.gridX * gridSize - gridOffset;
				let dY = y;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
		}

		// East pass
		count = 0;
		x = (s.gridX + 1) * gridSize - gridOffset;
		for (let y = s.gridY * gridSize + gridOffset; y < (s.gridY + 1) * gridSize; y += gridSize / nodesPerAxis) {
			let step = (gridSize / nodesPerAxis) * count++;
			let origin = worldToGPS(x, y, gpsBounds);
			// Override East transfer
			if (s.OverrideDestEastX > -1) {
				let dX = s.OverrideDestEastX * gridSize + gridOffset;
				let dY = s.OverrideDestEastY * gridSize + gridOffset + step;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
			// Wrap East Transfer
			else if (bounds.MaxX === s.gridX && bounds.MinX != bounds.MaxX) {
				let dX = bounds.MinX * gridSize + gridOffset;
				let dY = y;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
			// East Transfer
			else if (bounds.MinX != bounds.MaxX) {
				let dX = (s.gridX + 1) * gridSize + gridOffset;
				let dY = y;
				let destination = worldToGPS(dX, dY, gpsBounds);
				g.addLink(origin.toString(), destination.toString());
			}
		}
	}

	// Pass 3: link up portals
	if (serverConfig.portalPaths) {
		serverConfig.portalPaths.forEach((path) => {
			for (let i = 1; i < path.Nodes.length; i++) {
				let n1 = path.Nodes[0];
				let n2 = path.Nodes[i];
				let n1loc = [n1.worldX, n1.worldY];
				let n2loc = [n2.worldX, n2.worldY];
				if (n1.RequiredResource && Object.keys(n1.RequiredResource).length > 0) continue; // skip portals with required resources
				if (n2.RequiredResource && Object.keys(n2.RequiredResource).length > 0) continue; // skip portals with required resources
				// Link the closest nodes to the portal
				if (i === 1) linkNodesNearest(n1loc, g);
				linkNodesNearest(n2loc, g);

				// link the portals
				switch (path.PathPortalType) {
					case 0: // Both directions
						g.addLink(worldToGPS(n1loc[0], n1loc[1], gpsBounds).toString(), worldToGPS(n2loc[0], n2loc[1], gpsBounds).toString());
						g.addLink(worldToGPS(n2loc[0], n2loc[1], gpsBounds).toString(), worldToGPS(n1loc[0], n1loc[1], gpsBounds).toString());

						break;
					case 1: // One way
					case 2: // One way
					case 3: // One way
						g.addLink(worldToGPS(n1loc[0], n1loc[1], gpsBounds).toString(), worldToGPS(n2loc[0], n2loc[1], gpsBounds).toString());
						break;
				}
			}
		});
	}

	let pathfinder = [];

	g.forEachLink(function (link) {
		pathfinder.push({f: link.fromId, t: link.toId});
	});
	fs.writeFileSync("./json/pathfinder.json", JSON.stringify(pathfinder));

	function linkNodesNearest([x1, y1], g) {
		for (let i = 0; i < 4; i++) {
			let origin, destination;

			destination = worldToGPS(x1, y1, gpsBounds).toString();
			[x1, y1] = closestNode([x1, y1]);
			switch (i) {
				case 0:
					origin = worldToGPS(x1 + gridOffset, y1 + gridOffset, gpsBounds).toString();
					break;
				case 1:
					origin = worldToGPS(x1 + gridOffset, y1 - gridOffset, gpsBounds).toString();
					break;
				case 2:
					origin = worldToGPS(x1 - gridOffset, y1 - gridOffset, gpsBounds).toString();
					break;
				case 3:
					origin = worldToGPS(x1 - gridOffset, y1 + gridOffset, gpsBounds).toString();
					break;
			}

			g.addNode(destination);
			g.addLink(origin, destination);
			g.addLink(destination, origin);
		}
	}

	function worldToGPS(x, y, bounds) {
		let long = (x / worldUnitsX) * Math.abs(bounds.min[0] - bounds.max[0]) + bounds.min[0];
		let lat = bounds.min[1] - (y / worldUnitsY) * Math.abs(bounds.min[1] - bounds.max[1]);
		return [parseFloat(long.toFixed(1)), parseFloat(lat.toFixed(1))];
	}

	function GPSToWorld(x, y, bounds) {
		let long = ((x - bounds.min[0]) / Math.abs(bounds.min[0] - bounds.max[0])) * worldUnitsX;
		let lat = ((-y + bounds.min[1]) / Math.abs(bounds.min[1] - bounds.max[1])) * worldUnitsY;
		return [parseInt(long.toFixed(0)), parseInt(lat.toFixed(0))];
	}

	function sortObjByKey(value) {
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
	}

	function gridName(server) {
		return String.fromCharCode(65 + server[0]) + (server[1] + 1);
	}

	function closestNode(l) {
		return [roundNodeLocation(l[0]), roundNodeLocation(l[1])];
	}

	function roundNodeLocation(v) {
		let step = gridSize / nodesPerAxis;
		if (v % step === 0) {
			return Math.floor(v / step) * step;
		}
		return Math.floor(v / step) * step + step;
	}

	function getRegionBounds(x, y, regions) {
		let b;
		Object.entries(regions).forEach(([region, bounds]) => {
			if (x >= bounds.MinX && x <= bounds.MaxX && y >= bounds.MinY && y <= bounds.MaxY) {
				b = bounds;
				return true;
			}
		});
		return b;
	}
}

main();
