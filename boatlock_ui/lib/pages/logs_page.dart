import 'package:flutter/material.dart';
import '../services/log_service.dart';

class LogsPage extends StatelessWidget {
  const LogsPage({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("Логи устройства")),
      body: AnimatedBuilder(
        animation: logService,
        builder: (context, _) {
          return ListView.builder(
            itemCount: logService.logs.length,
            itemBuilder: (context, index) {
              return Text(
                logService.logs[index],
                style: TextStyle(fontFamily: 'monospace', fontSize: 13),
              );
            },
          );
        },
      ),
      floatingActionButton: FloatingActionButton(
        child: Icon(Icons.delete),
        tooltip: "Очистить логи",
        onPressed: () => logService.clear(),
      ),
    );
  }
}
