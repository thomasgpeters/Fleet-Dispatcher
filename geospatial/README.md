# Geospatial endpoint

The **spatial half of Feature 2** (truck locations & nearest-POI): a small
FastAPI service that runs PostGIS `ST_*` queries over the **`gis`** schema. It is
kept **off the ApiLogicServer path** by design — see
[`../docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md`](../docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md).

It connects as the **`fleet_gis`** role (created by `database/gis_bootstrap.sql`),
which owns the gis views and has `search_path = gis, fleet, public` preset.

> Deployment note: VCP builds the C++ and TS apps; this Python service — like the
> ApiLogicServer middleware — is deployed on its own (the included `Dockerfile`,
> or a systemd unit / `uvicorn`).

## Endpoints

| Method · Path                                   | Returns                                   |
| ----------------------------------------------- | ----------------------------------------- |
| `GET /health`                                   | `{status, postgis}`                       |
| `GET /truck-stops/nearest?lat=&lng=&limit=`     | nearest truck-stop POIs (KNN, `<->`)      |
| `GET /trucks/near?lat=&lng=&radius_m=`          | latest position per rig within a radius   |
| `GET /positions/latest`                         | latest position per rig (fleet-wide)      |

## Run

```bash
cd geospatial
pip install -r requirements.txt
cp .env.example .env                 # point GIS_DATABASE_URL at your DB
GIS_DATABASE_URL="postgresql://fleet_gis:fleet_gis@localhost:5432/fleet_dispatcher" \
  uvicorn app.main:app --host 0.0.0.0 --port 5701

# or containerized:
docker build -t fleet-geospatial .
docker run -e GIS_DATABASE_URL=... -p 5701:5701 fleet-geospatial
```

Prereqs: the DB stood up with PostGIS (`database/gis_bootstrap.sql`); see
[`../docs/DEPLOYMENT.md`](../docs/DEPLOYMENT.md).

## Example

```bash
curl "http://localhost:5701/truck-stops/nearest?lat=35.20&lng=-101.83&limit=3"
# [{"id":"…","name":"Big Rig Travel Center","miles":1}]
```
