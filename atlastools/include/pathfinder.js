const graph = require('ngraph.graph');
const fs = require('fs');
const helpers = require('./helpers');

function buildPathfinderGraph(serverConfig, gridSize, gridOffset, nodesPerAxis, gpsBounds, worldToGPS, getRegionBounds, closestNode) {
    let g = graph();

    // Pass 1: build nodes and links
    console.log("pathfinding pass 1...");
    for (let server in serverConfig.servers) {
        let s = serverConfig.servers[server];
        if (s.isMawWatersServer) continue;

        // loop west to east
        for (let x = s.gridX * gridSize + gridOffset; x < (s.gridX + 1) * gridSize; x += gridSize / nodesPerAxis) {
            let lastY;
            for (let y = s.gridY * gridSize + gridOffset; y < (s.gridY + 1) * gridSize; y += gridSize / nodesPerAxis) {
                let point = [x, y];
                nodeSearch: for (let island in s.islandInstances) {
                    if (helpers.inside(s.islandInstances[island], point)) {
                        lastY = undefined;
                        continue nodeSearch;
                    }
                }

                if (lastY !== undefined) {
                    g.addLink(worldToGPS(x, y, gpsBounds).toString(), worldToGPS(x, lastY, gpsBounds).toString());
                    g.addLink(worldToGPS(x, lastY, gpsBounds).toString(), worldToGPS(x, y, gpsBounds).toString());
                    if (x > s.gridX * gridSize + gridOffset * 10)
                        if (y > s.gridY * gridSize + gridOffset * 10) 
                            if (y < (s.gridY + 1) * gridSize - gridOffset * 10) 
                                if (x < (s.gridX + 1) * gridSize - gridOffset * 10) 
                                    linkNodesNearest([x, y], g);
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
    console.log("pathfinding pass 2...");
    const region = helpers.parseJSONFile("./json/regions.json");
    for (let server in serverConfig.servers) {
        let s = serverConfig.servers[server];
        if (s.isMawWatersServer) continue;

        let bounds = getRegionBounds(s.gridX, s.gridY, region);
        if (bounds === undefined) continue;

        processGridBorders(s, bounds, g);
    }

    // Pass 3: link up portals 
    if (serverConfig.portalPaths) {
        processPortals(serverConfig.portalPaths, g);
    }

    // Write pathfinder data
    let pathfinder = [];
    console.log("pathfinding build...");
    g.forEachLink(function(link) {
        pathfinder.push({f: link.fromId, t: link.toId});
    });
    fs.writeFileSync("./json/pathfinder.json", JSON.stringify(pathfinder));
    console.log("pathfinding complete.");

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

            g.addLink(origin, destination);
            g.addLink(destination, origin);
        }
    }

    function processGridBorders(s, bounds, g) {
        // Process each border direction (North, South, East, West)
        processNorthBorder(s, bounds, g);
        processSouthBorder(s, bounds, g); 
        processWestBorder(s, bounds, g);
        processEastBorder(s, bounds, g);
    }

    function processNorthBorder(s, bounds, g) {
        let y = s.gridY * gridSize + gridOffset;
        let count = 0;
        for (let x = s.gridX * gridSize + gridOffset; x < (s.gridX + 1) * gridSize; x += gridSize / nodesPerAxis) {
            let step = (gridSize / nodesPerAxis) * count++;
            let origin = worldToGPS(x, y, gpsBounds);

            if (s.OverrideDestNorthX > -1) {
                let dX = s.OverrideDestNorthX * gridSize + gridOffset + step;
                let dY = (s.OverrideDestNorthY + 1) * gridSize - gridOffset;
                let destination = worldToGPS(dX, dY, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
            else if (bounds.MinY === s.gridY && bounds.MinY != bounds.MaxY) {
                let destination = worldToGPS(x, (bounds.MaxY + 1) * gridSize - gridOffset, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
            else if (bounds.MinY != bounds.MaxY) {
                let destination = worldToGPS(x, s.gridY * gridSize - gridOffset, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
        }
    }

    function processSouthBorder(s, bounds, g) {
        let count = 0;
        let y = (s.gridY + 1) * gridSize - gridOffset;
        for (let x = s.gridX * gridSize + gridOffset; x < (s.gridX + 1) * gridSize; x += gridSize / nodesPerAxis) {
            let step = (gridSize / nodesPerAxis) * count++;
            let origin = worldToGPS(x, y, gpsBounds);

            if (s.OverrideDestSouthX > -1) {
                let dX = s.OverrideDestSouthX * gridSize + gridOffset + step;
                let dY = s.OverrideDestSouthY * gridSize + gridOffset;
                let destination = worldToGPS(dX, dY, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
            else if (bounds.MaxY === s.gridY && bounds.MinY != bounds.MaxY) {
                let destination = worldToGPS(x, bounds.MinY * gridSize + gridOffset, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
            else if (bounds.MinY != bounds.MaxY) {
                let destination = worldToGPS(x, (s.gridY + 1) * gridSize + gridOffset, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
        }
    }

    function processWestBorder(s, bounds, g) {
        let count = 0;
        let x = s.gridX * gridSize + gridOffset;
        for (let y = s.gridY * gridSize + gridOffset; y < (s.gridY + 1) * gridSize; y += gridSize / nodesPerAxis) {
            let step = (gridSize / nodesPerAxis) * count++;
            let origin = worldToGPS(x, y, gpsBounds);

            if (s.OverrideDestWestX > -1) {
                let dX = (s.OverrideDestWestX + 1) * gridSize - gridOffset;
                let dY = s.OverrideDestWestY * gridSize + gridOffset + step;
                let destination = worldToGPS(dX, dY, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
            else if (bounds.MinX === s.gridX && bounds.MinX != bounds.MaxX) {
                let destination = worldToGPS((bounds.MaxX + 1) * gridSize - gridOffset, y, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
            else if (bounds.MinX != bounds.MaxX) {
                let destination = worldToGPS(s.gridX * gridSize - gridOffset, y, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
        }
    }

    function processEastBorder(s, bounds, g) {
        let count = 0;
        let x = (s.gridX + 1) * gridSize - gridOffset;
        for (let y = s.gridY * gridSize + gridOffset; y < (s.gridY + 1) * gridSize; y += gridSize / nodesPerAxis) {
            let step = (gridSize / nodesPerAxis) * count++;
            let origin = worldToGPS(x, y, gpsBounds);

            if (s.OverrideDestEastX > -1) {
                let dX = s.OverrideDestEastX * gridSize + gridOffset;
                let dY = s.OverrideDestEastY * gridSize + gridOffset + step;
                let destination = worldToGPS(dX, dY, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
            else if (bounds.MaxX === s.gridX && bounds.MinX != bounds.MaxX) {
                let destination = worldToGPS(bounds.MinX * gridSize + gridOffset, y, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
            else if (bounds.MinX != bounds.MaxX) {
                let destination = worldToGPS((s.gridX + 1) * gridSize + gridOffset, y, gpsBounds);
                g.addLink(origin.toString(), destination.toString());
            }
        }
    }

    function processPortals(portalPaths, g) {
        portalPaths.forEach((path) => {
            for (let i = 1; i < path.Nodes.length; i++) {
                let n1 = path.Nodes[0];
                let n2 = path.Nodes[i];
                let n1loc = [n1.worldX, n1.worldY];
                let n2loc = [n2.worldX, n2.worldY];

                if (n1.RequiredResource && Object.keys(n1.RequiredResource).length > 0) continue;
                if (n2.RequiredResource && Object.keys(n2.RequiredResource).length > 0) continue;

                if (i === 1) linkNodesNearest(n1loc, g);
                linkNodesNearest(n2loc, g);

                switch (path.PathPortalType) {
                    case 0:
                        g.addLink(worldToGPS(n1loc[0], n1loc[1], gpsBounds).toString(), 
                                worldToGPS(n2loc[0], n2loc[1], gpsBounds).toString());
                        g.addLink(worldToGPS(n2loc[0], n2loc[1], gpsBounds).toString(),
                                worldToGPS(n1loc[0], n1loc[1], gpsBounds).toString());
                        break;
                    case 1:
                    case 2:
                    case 3:
                        g.addLink(worldToGPS(n1loc[0], n1loc[1], gpsBounds).toString(),
                                worldToGPS(n2loc[0], n2loc[1], gpsBounds).toString());
                        break;
                }
            }
        });
    }
}

module.exports = buildPathfinderGraph;