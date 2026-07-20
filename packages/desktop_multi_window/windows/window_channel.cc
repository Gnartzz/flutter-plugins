//
// Created by yangbin on 2022/1/27.
//

#include "window_channel.h"
#include "flutter/standard_method_codec.h"

#include <flutter_messenger.h>
#include <flutter_plugin_registrar.h>

#include <variant>

std::unique_ptr<WindowChannel>
WindowChannel::RegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar, int64_t window_id) {
  auto window_registrar = flutter::PluginRegistrarManager::GetInstance()
      ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar);
  auto channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
      window_registrar->messenger(), "mixin.one/flutter_multi_window_channel",
      &flutter::StandardMethodCodec::GetInstance());
  // Rohen Messenger dieses Engines mit AddRef festhalten (das Objekt ueberlebt
  // so den Engine, sodass wir im Destruktor gefahrlos IsAvailable pruefen
  // koennen, auch wenn der Channel erst beim CRT-atexit stirbt).
  auto messenger = FlutterDesktopPluginRegistrarGetMessenger(registrar);
  FlutterDesktopMessengerAddRef(messenger);
  return std::make_unique<WindowChannel>(window_id, std::move(channel), messenger);
}

WindowChannel::WindowChannel(
    int64_t window_id,
    std::unique_ptr<flutter::MethodChannel<Argument>> channel,
    FlutterDesktopMessengerRef messenger
) : window_id_(window_id), channel_(std::move(channel)), messenger_(messenger) {
  channel_->SetMethodCallHandler([this](const flutter::MethodCall<Argument> &call, auto result) {
    if (!handler_) {
      std::cout << "WindowChannel::SetMethodCallHandler: handler_ is null" << std::endl;
      return;
    }
    auto *args = std::get_if<flutter::EncodableMap>(call.arguments());
    auto target_window_id = args->at(flutter::EncodableValue("targetWindowId")).LongValue();
    auto arguments = args->at(flutter::EncodableValue("arguments"));
    handler_(window_id_, target_window_id, call.method_name(), &arguments, std::move(result));
  });
}

WindowChannel::~WindowChannel() {
  // Nur deregistrieren, solange der Engine (und damit sein Message-Dispatcher)
  // noch lebt. Beim App-Ende wird der statische MultiWindowManager erst im
  // CRT-atexit zerstoert — da ist der Engine laengst weg, und ein
  // SetMethodCallHandler(nullptr) wuerde in flutter_windows.dll abstuerzen
  // (der Client-Wrapper prueft die Verfuegbarkeit nur beim Senden, nicht beim
  // Deregistrieren). Bei laufendem Sub-Fenster (Runtime-Close) lebt der Engine
  // dagegen noch -> Handler wird sauber entfernt.
  if (messenger_ && FlutterDesktopMessengerIsAvailable(messenger_)) {
    channel_->SetMethodCallHandler(nullptr);
  }
  if (messenger_) {
    FlutterDesktopMessengerRelease(messenger_);
    messenger_ = nullptr;
  }
}

void WindowChannel::InvokeMethod(
    int64_t from_window_id, const std::string &method,
    WindowChannel::Argument *arguments,
    std::unique_ptr<flutter::MethodResult<Argument>> result
) {
  channel_->InvokeMethod(method, std::make_unique<flutter::EncodableValue>(
      flutter::EncodableMap{
          {flutter::EncodableValue("fromWindowId"), flutter::EncodableValue(from_window_id)},
          {flutter::EncodableValue("arguments"), *arguments},
      }
  ), std::move(result));
}

