class BoatData {
  final double lat;
  final double lon;
  final double anchorLat;
  final double anchorLon;
  final double anchorHeading;
  final double distance;
  final double anchorBearing;
  final double heading;
  final int battery;
  final String status;
  final String statusReasons;
  final String mode;
  final int rssi;
  final bool holdHeading;
  final int stepSpr;
  final double stepGear;
  final double stepMaxSpd;
  final double stepAccel;
  final double headingRaw;
  final double compassOffset;
  final int compassQ;
  final int magQ;
  final int gyroQ;
  final double rvAcc;
  final double magNorm;
  final double gyroNorm;
  final double pitch;
  final double roll;
  final int gnssQ;

  BoatData({
    required this.lat,
    required this.lon,
    required this.anchorLat,
    required this.anchorLon,
    required this.anchorHeading,
    required this.distance,
    required this.anchorBearing,
    required this.heading,
    required this.battery,
    required this.status,
    required this.statusReasons,
    required this.mode,
    required this.rssi,
    required this.holdHeading,
    required this.stepSpr,
    required this.stepGear,
    required this.stepMaxSpd,
    required this.stepAccel,
    required this.headingRaw,
    required this.compassOffset,
    required this.compassQ,
    required this.magQ,
    required this.gyroQ,
    required this.rvAcc,
    required this.magNorm,
    required this.gyroNorm,
    required this.pitch,
    required this.roll,
    required this.gnssQ,
  });
}
