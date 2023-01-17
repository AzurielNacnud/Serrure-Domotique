import 'package:flutter/material.dart';
import 'package:flutter_titled_container/flutter_titled_container.dart';

class SimpleLogWidget extends StatefulWidget {
  const SimpleLogWidget({Key? key}) : super(key: key);

  @override
  // ignore: library_private_types_in_public_api
  _SimpleLogWidgetState createState() => _SimpleLogWidgetState();
}

class _SimpleLogWidgetState extends State<SimpleLogWidget> {
  @override
  void initState() {
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    return TitledContainer(
      title: 'Successful logs',
      titleColor: Colors.green,
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
          child: SingleChildScrollView(
            child: Text('yes'),
          )),
    );
  }
}
