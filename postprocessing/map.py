import folium

FILE = "data_clean.txt"
OUT_HTML = "gps_map.html"

def load_gps(path):
    lat, lon = [], []

    with open(path, "r") as f:
        for line in f:
            parts = [p.strip() for p in line.split(",")]
            if len(parts) < 2:
                continue
            try:
                lat.append(float(parts[0]))
                lon.append(float(parts[1]))
            except:
                continue

    return lat, lon


lat, lon = load_gps(FILE)

if len(lat) == 0:
    raise ValueError("No GPS data")

center = [sum(lat)/len(lat), sum(lon)/len(lon)]

m = folium.Map(
    location=center,
    zoom_start=18,
    max_zoom=22,
    tiles=None
)

folium.TileLayer(
    tiles="https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
    attr="Esri",
    max_zoom=22,
    max_native_zoom=18,
    name="Satellite",
).add_to(m)

coords = list(zip(lat, lon))

folium.PolyLine(
    coords,
    weight=4
).add_to(m)

# Hiển thị các chấm điểm dữ liệu
for c in coords:
    folium.CircleMarker(
        location=c, 
        radius=2, 
        color="red", 
        fill=True, 
        fill_color="red"
    ).add_to(m)

folium.Marker(coords[0], tooltip="Start").add_to(m)
folium.Marker(coords[-1], tooltip="End").add_to(m)

folium.LayerControl().add_to(m)

m.save(OUT_HTML)

print(f"Saved to {OUT_HTML}")