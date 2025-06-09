class BoatData {
  final double lat;
  final double lon;
  final double anchorLat;
  final double anchorLon;
  final double distance;
  final double heading;
  final int battery;
  final String status;

  BoatData({
    required this.lat,
    required this.lon,
    required this.anchorLat,
    required this.anchorLon,
    required this.distance,
    required this.heading,
    required this.battery,
    required this.status,
  });

  factory BoatData.fromJson(Map<String, dynamic> json) {
    return BoatData(
      lat: double.tryParse(json['lat']?.toString() ?? '') ?? 0,
      lon: double.tryParse(json['lon']?.toString() ?? '') ?? 0,
      anchorLat: double.tryParse(json['anchorLat']?.toString() ?? '') ?? 0,
      anchorLon: double.tryParse(json['anchorLon']?.toString() ?? '') ?? 0,
      distance: double.tryParse(json['distance']?.toString() ?? '') ?? 0,
      heading: double.tryParse(json['heading']?.toString() ?? '') ?? 0,
      battery: int.tryParse(json['battery']?.toString() ?? '') ?? 0,
      status: json['status']?.toString() ?? '',
    );
  }
}
