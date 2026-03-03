class BoatData {
  final double lat;
  final double lon;
  final double anchorLat;
  final double anchorLon;
  final double anchorHeading;
  final double distance;
  final double heading;
  final double speedKmh;
  final int motorPwm;
  final int battery;
  final String status;
  final String mode;
  final int rssi;
  final bool holdHeading;
  final int stepSpr;
  final double stepMaxSpd;
  final double stepAccel;
  final double stepperDeg;
  final bool motorReverse;
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

  BoatData({
    required this.lat,
    required this.lon,
    required this.anchorLat,
    required this.anchorLon,
    required this.anchorHeading,
    required this.distance,
    required this.heading,
    required this.speedKmh,
    required this.motorPwm,
    required this.battery,
    required this.status,
    required this.mode,
    required this.rssi,
    required this.holdHeading,
    required this.stepSpr,
    required this.stepMaxSpd,
    required this.stepAccel,
    required this.stepperDeg,
    required this.motorReverse,
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
  });

  factory BoatData.fromJson(Map<String, dynamic> json, {int rssi = 0}) {
    return BoatData(
      lat: double.tryParse(json['lat']?.toString() ?? '') ?? 0,
      lon: double.tryParse(json['lon']?.toString() ?? '') ?? 0,
      anchorLat: double.tryParse(json['anchorLat']?.toString() ?? '') ?? 0,
      anchorLon: double.tryParse(json['anchorLon']?.toString() ?? '') ?? 0,
      anchorHeading: double.tryParse(json['anchorHead']?.toString() ?? '') ?? 0,
      distance: double.tryParse(json['distance']?.toString() ?? '') ?? 0,
      heading: double.tryParse(json['heading']?.toString() ?? '') ?? 0,
      speedKmh: double.tryParse(json['speedKmh']?.toString() ?? '') ?? 0,
      motorPwm: int.tryParse(json['motorPwm']?.toString() ?? '') ?? 0,
      battery: int.tryParse(json['battery']?.toString() ?? '') ?? 0,
      status: json['status']?.toString() ?? '',
      mode: json['mode']?.toString() ?? '',
      rssi: rssi,
      holdHeading: int.tryParse(json['holdHeading']?.toString() ?? '') == 1,
      stepSpr: int.tryParse(json['stepSpr']?.toString() ?? '') ?? 4096,
      stepMaxSpd: double.tryParse(json['stepMaxSpd']?.toString() ?? '') ?? 1000,
      stepAccel: double.tryParse(json['stepAccel']?.toString() ?? '') ?? 500,
      stepperDeg: double.tryParse(json['stepperDeg']?.toString() ?? '') ?? 0,
      motorReverse: int.tryParse(json['motorReverse']?.toString() ?? '') == 1,
      headingRaw: double.tryParse(json['headingRaw']?.toString() ?? '') ?? 0,
      compassOffset:
          double.tryParse(json['compassOffset']?.toString() ?? '') ?? 0,
      compassQ: int.tryParse(json['compassQ']?.toString() ?? '') ?? 0,
      magQ: int.tryParse(json['magQ']?.toString() ?? '') ?? 0,
      gyroQ: int.tryParse(json['gyroQ']?.toString() ?? '') ?? 0,
      rvAcc: double.tryParse(json['rvAcc']?.toString() ?? '') ?? 0,
      magNorm: double.tryParse(json['magNorm']?.toString() ?? '') ?? 0,
      gyroNorm: double.tryParse(json['gyroNorm']?.toString() ?? '') ?? 0,
      pitch: double.tryParse(json['pitch']?.toString() ?? '') ?? 0,
      roll: double.tryParse(json['roll']?.toString() ?? '') ?? 0,
    );
  }
}
