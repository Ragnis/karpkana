// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: colors.proto

package c22dlink;

public interface ColorInfoOrBuilder extends
    // @@protoc_insertion_point(interface_extends:c22dlink.ColorInfo)
    com.google.protobuf.MessageOrBuilder {

  /**
   * <code>required int32 id = 1;</code>
   */
  boolean hasId();
  /**
   * <code>required int32 id = 1;</code>
   */
  int getId();

  /**
   * <code>optional .c22dlink.RGB rgb = 2;</code>
   */
  boolean hasRgb();
  /**
   * <code>optional .c22dlink.RGB rgb = 2;</code>
   */
  c22dlink.RGB getRgb();
  /**
   * <code>optional .c22dlink.RGB rgb = 2;</code>
   */
  c22dlink.RGBOrBuilder getRgbOrBuilder();

  /**
   * <code>optional string name = 3;</code>
   */
  boolean hasName();
  /**
   * <code>optional string name = 3;</code>
   */
  java.lang.String getName();
  /**
   * <code>optional string name = 3;</code>
   */
  com.google.protobuf.ByteString
      getNameBytes();

  /**
   * <code>required double merge = 4;</code>
   */
  boolean hasMerge();
  /**
   * <code>required double merge = 4;</code>
   */
  double getMerge();

  /**
   * <code>required int32 expected = 5;</code>
   */
  boolean hasExpected();
  /**
   * <code>required int32 expected = 5;</code>
   */
  int getExpected();

  /**
   * <code>required .c22dlink.YUV yuvLow = 6;</code>
   */
  boolean hasYuvLow();
  /**
   * <code>required .c22dlink.YUV yuvLow = 6;</code>
   */
  c22dlink.YUV getYuvLow();
  /**
   * <code>required .c22dlink.YUV yuvLow = 6;</code>
   */
  c22dlink.YUVOrBuilder getYuvLowOrBuilder();

  /**
   * <code>required .c22dlink.YUV yuvHigh = 7;</code>
   */
  boolean hasYuvHigh();
  /**
   * <code>required .c22dlink.YUV yuvHigh = 7;</code>
   */
  c22dlink.YUV getYuvHigh();
  /**
   * <code>required .c22dlink.YUV yuvHigh = 7;</code>
   */
  c22dlink.YUVOrBuilder getYuvHighOrBuilder();
}
