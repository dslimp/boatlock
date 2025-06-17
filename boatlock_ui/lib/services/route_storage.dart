import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/route_model.dart';

class RouteStorage {
  static const _key = 'routes';

  Future<List<RouteModel>> loadRoutes() async {
    final prefs = await SharedPreferences.getInstance();
    final list = prefs.getStringList(_key) ?? [];
    return list
        .map((s) => RouteModel.fromJson(jsonDecode(s) as Map<String, dynamic>))
        .toList();
  }

  Future<void> saveRoutes(List<RouteModel> routes) async {
    final prefs = await SharedPreferences.getInstance();
    final list = routes.map((r) => jsonEncode(r.toJson())).toList();
    await prefs.setStringList(_key, list);
  }
}
