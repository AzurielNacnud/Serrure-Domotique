class Log {
  String code, date;

  Log({required this.code, required this.date});

  factory Log.fromJson(Map<String, dynamic> json) {
    return Log(code: json['code'], date: json['date']);
  }
}
