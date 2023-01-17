import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_titled_container/flutter_titled_container.dart';
import 'package:shared_preferences/shared_preferences.dart';

class CodesWidget extends StatefulWidget {
  const CodesWidget({super.key});

  @override
  State<CodesWidget> createState() => _CodesWidgetState();
}

class _CodesWidgetState extends State<CodesWidget> {
  late List<String> codelist = [];

  final TextEditingController _controller = TextEditingController();

  @override
  void initState() {
    loadData();
    super.initState();
  }

  @override
  void dispose() {
    super.dispose();
    _controller.dispose();
  }

  Future<void> loadData() async {
    SharedPreferences prefs = await SharedPreferences.getInstance();
    _controller.text = prefs.getString('codes') ?? '';
    setState(() {
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      resizeToAvoidBottomInset: true,
      appBar: AppBar(
        title: const Text('Codes'),
        centerTitle: true,
        leading: Container(),
      ),
      body: Center(
          child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: <Widget>[
/*
          TitledContainer(
              title: 'Codes',
              backgroundColor: Colors.grey[200],
              fontSize: 20.0,
              textAlign: TextAlignTitledContainer.Center,
              child: Container(
                height: MediaQuery.of(context).size.height / 1.5,
                width: MediaQuery.of(context).size.width / 1.1,
                decoration: BoxDecoration(
                  color: Colors.grey[200],
                  border: Border.all(color: Colors.green),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: TextFormField(
                  controller: _controller,
                  maxLines: 10,
                  decoration: const InputDecoration(
                    border: OutlineInputBorder(),
                    hintText: 'Enter codes',
                  ),
                ),
              )),*/
          _buildTextField(),
          _buildButton(),
        ],
      )),
    );
  }

  //Crée un widget pour ecrire les codes
  Widget _buildTextField() {
    return Column(
      children: [
        const Center(
          child: Text(
            'Codes',
            style: TextStyle(
              fontSize: 20.0,
              color: Colors.black,
            ),
          ),
        ),
        Container(
          height: 300,
          padding: const EdgeInsets.symmetric(vertical: 16.0),
          alignment: Alignment.center,
          child: TextFormField(
            controller: _controller,
            maxLines: 100,
            decoration: const InputDecoration(
              border: OutlineInputBorder(),
              hintText: 'Enter codes',
            ),
          ),
        ),
      ],
    );
  }

  //stocker les codes dans la memoire

  // créer un widget button pour envoyer les codes
  Widget _buildButton() {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 16.0),
      width: double.infinity,
      child: ElevatedButton(
        style: ElevatedButton.styleFrom(
          padding: const EdgeInsets.all(12.0),
          primary: Colors.green,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(30.0),
          ),
        ),
        onPressed: () {
          sendPasswords();
        },
        child: const Text(
          'Send',
          style: TextStyle(color: Colors.white, fontSize: 20.0),
        ),
      ),
    );
  }

  Future<void> sendPasswords() async {
    SharedPreferences prefs = await SharedPreferences.getInstance();
    await prefs.setString('codes', _controller.text);

    String txt = _controller.text;
    List<String> txts = txt.split('\n');

    codelist = txts;

    String temp = "";

    for (String str in codelist) {
      if (codelist.last == str) {
        temp += str;
      } else {
        temp += "$str,";
      }
    }

    var data = {'name': 'changePwds,$temp'};
    apiRequest(
        'https://serrurefunction.azurewebsites.net/api/HttpTrigger?code=xHh6RZa_DSQuDRvX3Fg6pKw_UbxKa5-8J6tcV-cl6BuEAzFuArLBsg==',
        data);
  }

  Future<String> apiRequest(String url, Map jsonMap) async {
    HttpClient httpClient = new HttpClient();
    HttpClientRequest request = await httpClient.postUrl(Uri.parse(url));
    request.headers.set('content-type', 'application/json');
    request.add(utf8.encode(json.encode(jsonMap)));
    HttpClientResponse response = await request.close();
    // todo - check the response.statusCode
    String reply = await response.transform(utf8.decoder).join();
    httpClient.close();
    return reply;
  }
}
