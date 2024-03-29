const fs = require("fs");

const helpers = require("./atlastools/include/helpers.js");
const baseDir = "DumpResources/server/ShooterGame/";
const serverConfig = helpers.parseJSONFile(baseDir + "ServerGrid.json");

const mapIsland = ["Cay_K_TR_E", "Cay_K_WR", "Cay_K_WR_B", "Cay_L_CL", "Cay_L_CL_B", "Cay_L_EE", "Cay_L_ER", "Cay_L_WR", "Cay_L_WR_B", "Cay_L_CL_B"];

console.log("## Golden Age Islands & No builds");
console.log("## Should be all trench, GA, Freeports");

for (let server in serverConfig.servers) {
	let s = serverConfig.servers[server];

	if (s.forceServerRules === 5) {
		for (let island in s.islandInstances) {
			let i = s.islandInstances[island];
			console.log(helpers.gridName(s.gridX, s.gridY), i.name, i.islandPoints);
		}
	} else {
		for (let island in s.islandInstances) {
			let i = s.islandInstances[island];
			if (i.islandPoints === -1) console.log(helpers.gridName(s.gridX, s.gridY), i.name, i.islandPoints);
		}
	}
}

console.log("\n\n## Boosted Maps");
for (let server in serverConfig.servers) {
	let s = serverConfig.servers[server];

	for (let island in s.islandInstances) {
		let i = s.islandInstances[island];
		if (i.instanceTreasureQualityMultiplier > 1.0 || i.instanceTreasureQualityAddition > 1.0)
			console.log(helpers.gridName(s.gridX, s.gridY), i.name, i.instanceTreasureQualityMultiplier, i.instanceTreasureQualityAddition, s.hiddenAtlasId);
	}
}

console.log("\n\n## Nerfed Maps");
for (let server in serverConfig.servers) {
	let s = serverConfig.servers[server];

	for (let island in s.islandInstances) {
		let i = s.islandInstances[island];
		if (i.instanceTreasureQualityMultiplier < 1.0 || i.instanceTreasureQualityAddition < 1.0)
			console.log(helpers.gridName(s.gridX, s.gridY), i.name, i.instanceTreasureQualityMultiplier, i.instanceTreasureQualityAddition, s.hiddenAtlasId);
	}
}


console.log("\n\n## HQ Map Islands");
for (let server in serverConfig.servers) {
	let s = serverConfig.servers[server];

	for (let island in s.islandInstances) {
		let i = s.islandInstances[island];
		if (mapIsland.includes(i.name) && s.hiddenAtlasId) console.log(helpers.gridName(s.gridX, s.gridY), i.name, s.hiddenAtlasId, s.forceServerRules);
	}
}

console.log("\n\n## Boosted Flotsam & Wrecks & SotD");
for (let server in serverConfig.servers) {
	let s = serverConfig.servers[server];
	if (s.ServerCustomDatas1) {
		let k = s.ServerCustomDatas1.split(",");
		let v = s.ServerCustomDatas2.split(",");
		for (let i = 1; i < k.length; i++) {
			//if (k[i] === "SunkenTreasureQualityMultiplier" && v[i] > 1.0) console.log(helpers.gridName(s.gridX, s.gridY), k[i], v[i]);
			//if (k[i] === "FloatsamQualityMultiplier" && v[i] > 1.5) console.log(helpers.gridName(s.gridX, s.gridY), k[i], v[i]);
			if (k[i] === "NPCShipDifficultyMult" && v[i] > 1.0) console.log(helpers.gridName(s.gridX, s.gridY), k[i], v[i]);
		}
	}
}

console.log("\n\n## Claimable Islands");
for (let server in serverConfig.servers) {
	let s = serverConfig.servers[server];
	if (s.forceServerRules !== 3) continue;

	for (let island in s.islandInstances) {
		let i = s.islandInstances[island];
		if (i.name === "ControlPoint" || i.name.startsWith("Ice_")) continue;
		console.log(helpers.gridName(s.gridX, s.gridY), i.name, s.hiddenAtlasId, s.forceServerRules);
	}
}

console.log("\n\n## Spawnable Settlement Islands");
for (let server in serverConfig.servers) {
	let s = serverConfig.servers[server];

	if (s.forceServerRules === 3 && s.isHomeServer) console.log(helpers.gridName(s.gridX, s.gridY), s.hiddenAtlasId, s.forceServerRules);
}


console.log("\n\n# Land Node Keys");
for (let server in serverConfig.servers) {
	let s = serverConfig.servers[server];

	for (let island in s.islandInstances) {
		let i = s.islandInstances[island];
		if (i.landNodeKey) console.log(helpers.gridName(s.gridX, s.gridY), i.landNodeKey, i.name, s.hiddenAtlasId);
	}
}

