import 'package:flutter/material.dart';
import '../services/log_service.dart';

class LogsPage extends StatefulWidget {
  const LogsPage({Key? key}) : super(key: key);

  @override
  State<LogsPage> createState() => _LogsPageState();
}

class _LogsPageState extends State<LogsPage> {
  final ScrollController _scrollController = ScrollController();

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scrollController.hasClients) {
        _scrollController.jumpTo(_scrollController.position.maxScrollExtent);
      }
    });
  }

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("Логи устройства")),
      body: AnimatedBuilder(
        animation: logService,
        builder: (context, _) {
          _scrollToBottom();
          return ListView.builder(
            controller: _scrollController,
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
