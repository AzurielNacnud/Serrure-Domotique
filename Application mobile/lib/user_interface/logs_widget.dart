import 'dart:async';
import 'dart:convert';

import 'package:azblob/azblob.dart';
import 'package:flutter/material.dart';
import 'package:flutter_titled_container/flutter_titled_container.dart';

class LogsWidget extends StatefulWidget {
  const LogsWidget({super.key});

  @override
  State<LogsWidget> createState() => _HomeWidgetState();
}

class _HomeWidgetState extends State<LogsWidget> {
  Timer? timer;

  late List<ItemData> _items = [];
  late List<ItemData> _failed = [];
  late List<ItemData> _successful = [];

  @override
  void initState() {
    super.initState();
    refreshData();
    timer = Timer.periodic(const Duration(seconds: 5), (timer) {
      print("Performed");
      refreshData();
    });
  }

  @override
  void dispose() {
    timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Logs'),
        centerTitle: true,
        leading: Container(),
      ),
      body: Center(
          child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: <Widget>[
          TitledContainer(
            title: 'Successful logs',
            titleColor: Colors.green,
            backgroundColor: Colors.green[50],
            fontSize: 20.0,
            textAlign: TextAlignTitledContainer.Center,
            child: Container(
                height: MediaQuery.of(context).size.height / 3,
                width: MediaQuery.of(context).size.width / 1.1,
                decoration: BoxDecoration(
                  color: Colors.green[50],
                  border: Border.all(color: Colors.green),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: ListView.builder(
                    itemCount: _successful.length,
                    itemBuilder: (context, index) {
                      return buildItem(_successful[index]);
                    })),
          ),
          const SizedBox(height: 20),
          TitledContainer(
            title: 'Failed logs',
            titleColor: Colors.red,
            backgroundColor: Colors.red[50],
            fontSize: 20.0,
            textAlign: TextAlignTitledContainer.Center,
            child: Container(
                height: MediaQuery.of(context).size.height / 3,
                width: MediaQuery.of(context).size.width / 1.1,
                decoration: BoxDecoration(
                  color: Colors.red[50],
                  border: Border.all(color: Colors.red),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: Container(
                  child: ListView.builder(
                      itemCount: _failed.length,
                      itemBuilder: (context, index) {
                        return buildItem(_failed[index]);
                      }),
                )),
          ),
        ],
      )),
    );
  }

  Widget buildItem(ItemData itemData) {
    return Container(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          SizedBox(height: 10),
          Text(
              "[${itemData.day}/${itemData.month}/${itemData.year} - ${itemData.hour}:${itemData.minute}] code utilisé ${itemData.code}"),
        ],
      ),
    );
  }

  Future<List<ItemData>> showBlobStorage() async {
    final _connectionString =
        "DefaultEndpointsProtocol=https;AccountName=serruredomotique;AccountKey=uUH0sJUiWp9E1MjLfWCo5GFYx1q8mUSMBQNfARrsvxQffooW1MGHk+FEGZzrC1gny1PAFrdPPusz+AStTOTPlg==;EndpointSuffix=core.windows.net";
    var storage = AzureStorage.parse(_connectionString);

    var result = await storage
        .getBlob(
            '/serruredomotique/ESP8266/0_6850b15dd17d4b3192954b55c91f8658_1.json')
        .asStream()
        .last;
    final response = await result.stream.bytesToString();

    final firsChar = '[';
    final lastChar = '{}]';

    final body =
        firsChar + response.toString().replaceAll("}}", "}},") + lastChar;
    final json = jsonDecode(body);

    List<ItemData> items = [];

    for (var item in json) {
      if (item['code'] == null) continue;

      bool state = item['state'].toString() == "1" ? true : false;

      String codeStr = item['code'].toString();
      String dayStr = item['day'].toString();
      String monthStr = item['month'].toString();
      String yearStr = item['year'].toString();
      String hourStr = item['hour'].toString();
      String minuteStr = item['minute'].toString();

      int code = int.parse(codeStr);
      int day = int.parse(dayStr);
      int month = int.parse(monthStr);
      int year = int.parse(yearStr);
      int hour = int.parse(hourStr);
      int minute = int.parse(minuteStr);

      items.add(ItemData(state, code, day, month, year + 1900, hour, minute));
    }

    return items;
  }

  void refreshData() async {
    _items.clear();

    _items = await showBlobStorage();

    List<ItemData> _failedtemp = [];
    List<ItemData> _successfultemp = [];

    _failed.clear();
    _successful.clear();

    for (var item in _items) {
      if (item.state == true) {
        _successfultemp.add(item);
      } else {
        _failedtemp.add(item);
      }
    }

    _successful = _successfultemp.reversed.toList();
    _failed = _failedtemp.reversed.toList();

    setState(() {});
  }
}

class ItemData {
  final bool state;
  final int code;
  final int day;
  final int month;
  final int year;
  final int hour;
  final int minute;

  ItemData(this.state, this.code, this.day, this.month, this.year, this.hour,
      this.minute);
}
