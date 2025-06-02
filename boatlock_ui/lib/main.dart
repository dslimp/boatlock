// lib/main.dart
import 'package:flutter/material.dart';
import 'boatlock_page.dart';
import 'status_page.dart';

void main() {
  runApp(const BoatLockApp());
}

class BoatLockApp extends StatelessWidget {
  const BoatLockApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BoatLock BLE Control',
      theme: ThemeData(primarySwatch: Colors.blue),
      home: const MainScreen(),
    );
  }
}

class MainScreen extends StatefulWidget {
  const MainScreen({super.key});
  @override
  State<MainScreen> createState() => _MainScreenState();
}

class _MainScreenState extends State<MainScreen> {
  int _selectedIndex = 0;

  final List<Widget> _pages = [
    const ControlPage(),
    const StatusPage(),
  ];

  void _onItemTapped(int index) => setState(() => _selectedIndex = index);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: _pages[_selectedIndex],
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _selectedIndex,
        onTap: _onItemTapped,
        items: const [
          BottomNavigationBarItem(icon: Icon(Icons.tune), label: 'Управление'),
          BottomNavigationBarItem(icon: Icon(Icons.insights), label: 'Статус'),
        ],
      ),
    );
  }
}
