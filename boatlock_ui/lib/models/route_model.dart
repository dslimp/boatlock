import 'package:latlong2/latlong.dart';

class RouteModel {
  final String name;
  final List<LatLng> points;

  RouteModel({required this.name, required this.points});

  Map<String, dynamic> toJson() => {
        'name': name,
        'points': points
            .map((p) => {'lat': p.latitude, 'lon': p.longitude})
            .toList(),
      };

  factory RouteModel.fromJson(Map<String, dynamic> json) {
    final pts = (json['points'] as List?) ?? [];
    return RouteModel(
      name: json['name'] as String? ?? '',
      points: pts
          .map((e) => LatLng(
              (e['lat'] as num).toDouble(), (e['lon'] as num).toDouble()))
          .toList(),
    );
  }
}
